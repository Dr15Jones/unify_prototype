#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/TransitionsDistributorGuard.h"
#include "TransitionHandling/FilesProcessor.h"
#include "TransitionHandling/AsyncActionBase.h"
#include "Concurrency/chain_first.h"
#include "Concurrency/WaitingTaskList.h"

#include <map>

namespace edm {
  ConcurrentTransitionScheduler::SupporterResource::~SupporterResource() {
    if (resource_ && scheduler_) {
      auto key = resource_->key_;
      auto recordID = resource_->recordID_;
      scheduler_->endSupporterTransitionAsync(key, recordID, resource_->processEndTask_);
    }
  }

  //Must only be called by TransitionsDistributor (as it serializes the calls to readAsync and tryToMergeAsync)
  void ConcurrentTransitionScheduler::doneProcessing() {
    //NEED to release supports in correct order (from most nested to least nested), so have to do this one at a time and not in a loop.
    std::map<std::size_t, edm::TransitionRecordKey, std::greater<>> orderedSupporters;
    for (auto& [_, resource] : supporterResources_) {
      if (resource) {
        orderedSupporters.emplace(resource->resource_->recordID_.size(), resource->resource_->key_);
      }
    }
    for (auto& [_, key] : orderedSupporters) {
      auto it = supporterResources_.find(key);
      if (it != supporterResources_.end()) {
        it->second.reset();
      }
    }
    //we want the endProcessAsync to be the next to run on this thread so has to be done last.
    transitionResource_.reset();
  }
  void ConcurrentTransitionScheduler::readAsync(edm::SourceCoordinator& coordinator,
                                                edm::TransitionRecordID const& recordID,
                                                edm::ConcurrentTransitionID& oTransitionID,
                                                edm::WaitingTaskHolder lastTask,
                                                edm::WaitingTaskHolder nextTask) {
    transitionResource_.reset();
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newSupporterTransitionComing(transition_);
    }

    std::shared_ptr<FileTransitionResource const> fileResource = fileTransitionResource_.lock();
    assert(fileResource);
    queue_.pushAndPause(*nextTask.group(),
                        [this,
                         &coordinator,
                         recordID,
                         fileResource,
                         &oTransitionID,
                         holder = std::move(nextTask),
                         lastTask = std::move(lastTask)](auto iResumer, size_t index) mutable {
                          edm::ConcurrentTransitionID streamID(index);
                          oTransitionID = streamID;

                          if (*failureDuringProcessing_) {
                            try {
                              throw std::runtime_error("Aborting readAsync due to previous failure");
                            } catch (...) {
                              holder.doneWaiting(std::current_exception());
                            }
                            return;
                          }
                          concurrentRecords_[index] = recordID;
                          announceNewTransitionComing(streamID, recordID, std::move(iResumer), std::move(lastTask));
                          holdResources(streamID);
                          beginTransitionRan_[streamID.id()] = 0;

                          coordinator.readTransitionAsync(transition_, streamID, std::move(holder));
                        });
  }

  void ConcurrentTransitionScheduler::processAsync(TransitionsDistributorGuard&& distributor,
                                                   std::unique_ptr<edm::ConcurrentTransitionID> activeStream,
                                                   edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;

    std::shared_ptr<FileTransitionResource const> fileResource = fileTransitionResource_.lock();
    auto* pActiveStream = activeStream.get();

    //Need to wait till conditions are ready before we can start the beginTransitionAsync since it may need to read conditions.
    chain::first([this, distributor = std::move(distributor), finalHolder = holder, pActiveStream](
                     std::exception_ptr const* ptr, edm::WaitingTaskHolder holder) mutable {
      if (ptr or *failureDuringProcessing_) {
        failureDuringProcessing_->store(true);
        distributor.release();
        if (ptr) {
          holder.doneWaiting(*ptr);
        }
        return;
      }
      //we can release the distributor and allow the beginGlobalAsync to happen concurrently.
      //NOTE: we need to enqueue to the dependent queues BEFORE releasing the distributor.
      // However, we want the dependent TBB tasks to be added AFTER the distributor adds its task.
      announceNewTransitionAvailable(*pActiveStream, finalHolder);
      //this can't throw so do not need to guard next call
      distributor.release();
      announceDistributorReleased();
      // Simulate processing the transition here
      beginTransitionRan_[pActiveStream->id()] = 1;
      processBeginAsync(*pActiveStream, std::move(holder));
    }) |
        chain::then([this, fileResource = std::move(fileResource), stream = std::move(activeStream)](
                        std::exception_ptr const* ptr, edm::WaitingTaskHolder holder) mutable {
          std::exception_ptr localPtr;
          if (ptr) {
            localPtr = *ptr;
            failureDuringProcessing_->store(true);
          }

          // tell any waiting dependent transitions that the record is now available
          waitingDependentTransitionTasks_[stream->id()].doneWaiting(localPtr);
          holder.doneWaiting(localPtr);
        }) |
        chain::runLast(std::move(holder));
  }

  void ConcurrentTransitionScheduler::newSupporterTransitionComing(edm::TransitionRecordKey transitionKey) {
    for (auto* scheduler : dependentSchedulers_) {
      //tell dependents we are also ending. Do this first to get the ordering right in the dependents (from most nested to least nested).
      scheduler->newSupporterTransitionComing(transition_);
      //pass this on
      scheduler->newSupporterTransitionComing(transitionKey);
    }
    auto findIt = supporterResources_.find(transitionKey);
    assert(findIt != supporterResources_.end());
    findIt->second.reset();
    transitionResource_.reset();
  }
  void ConcurrentTransitionScheduler::announceDistributorReleased() {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->resumeBeginSupporterTransitionAsync(transition_);
    }
  }
  void ConcurrentTransitionScheduler::resumeBeginSupporterTransitionAsync(
      edm::TransitionRecordKey const& transitionKey) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->resumeBeginSupporterTransitionAsync(transitionKey);
    }
    if (supporterBeginActions_.find(transitionKey) == supporterBeginActions_.end() &&
        supporterEndActions_.find(transitionKey) == supporterEndActions_.end()) {
      //we didn't pause the queue for this transition since there are no actions.
      return;
    }
    queue_.resumeAll();
  }

  //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::newSupporterTransitionResource(
      edm::TransitionRecordKey transitionKey, std::shared_ptr<ConcurrentTransitionResource const> resource) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newSupporterTransitionResource(transitionKey, resource);
    }
    //if a new dependent is coming than this resource needs to be closed out
    transitionResource_.reset();
    auto findIt = supporterResources_.find(transitionKey);
    assert(findIt != supporterResources_.end());
    findIt->second = std::make_shared<SupporterResource>(std::move(resource));
  }

  void ConcurrentTransitionScheduler::announceNewTransitionAvailable(edm::ConcurrentTransitionID index,
                                                                     edm::WaitingTaskHolder holder) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newSupporterTransitionAvailable(transition_, waitingDependentTransitionTasks_[index.id()], holder);
    }
  }

  void ConcurrentTransitionScheduler::newSupporterTransitionAvailable(edm::TransitionRecordKey transitionKey,
                                                                      edm::WaitingTaskList& waitingTasks,
                                                                      edm::WaitingTaskHolder holder) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newSupporterTransitionAvailable(transitionKey, waitingTasks, holder);
    }

    if (supporterBeginActions_.find(transitionKey) == supporterBeginActions_.end() &&
        supporterEndActions_.find(transitionKey) == supporterEndActions_.end()) {
      //no actions for this transition, we can just add the task to the waiting list and be done with it.
      waitingTasks.add(std::move(holder));
      return;
    }

    auto findIt = supporterResources_.find(transitionKey);
    assert(findIt != supporterResources_.end());
    //copy fine here since std::shared_ptr is reference counted so destructor of SupporterResource will only be called when all copies are gone.
    auto resource = findIt->second;
    assert(resource);
    //NOTE: the resource->processEndTask_ holds the task to run end transition, therefore the Transition data alive until it is run (even though the ConcurrentTransitionResource is destroyed).
    //this will cause endSupporterTransitionAsync to be called
    resource->scheduler_ = this;
    auto recordID = resource->resource_->recordID_;
    // We need these enqueued while the distributor is being held to be sure all ConcurrrentTransitions agree on the order of supporter transitions and so no other
    // transition can sneak into the queue before this one.
    pauseAndEnqueueBeginSupporterTransitionAsync(transitionKey, recordID, waitingTasks, std::move(holder));
  }

  void ConcurrentTransitionScheduler::newFileComing(std::weak_ptr<FileTransitionResource const> resource) {
    fileTransitionResource_ = std::move(resource);
  }

  void ConcurrentTransitionScheduler::tryToMergeAsync(edm::SourceCoordinator& coordinator,
                                                      edm::TransitionRecordID const& recordID,
                                                      edm::WaitingTaskHolder holder) {
    // This is called for the first file open and we haven't read the transition yet.
    if (transitionResource_ and recordID == transitionResource_->recordID_) {
      coordinator.mergeTransitionAsync(transition_, recordID, transitionResource_->replica_, std::move(holder));
    }
  }

  //called only when TransitionDistributor is paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::holdResources(edm::ConcurrentTransitionID id) {
    std::size_t i = 0;
    for (auto& [_, resource] : supporterResources_) {
      if (resource) {
        concurrentHeldSupporterResources_[id.id()][i] = resource->resource_;
      }
      ++i;
    }
  }
  void ConcurrentTransitionScheduler::releaseResources(edm::ConcurrentTransitionID id) {
    for (auto& resource : concurrentHeldSupporterResources_[id.id()]) {
      resource.reset();
    }
  }
  //Called while the TransitionDistributor is still paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::announceNewTransitionComing(edm::ConcurrentTransitionID index,
                                                                  edm::TransitionRecordID const& recordID,
                                                                  edm::ConcurrentTransitionsTaskQueue::Resumer resumer,
                                                                  edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;
    waitingDependentTransitionTasks_[index.id()].reset();
    auto task = chain::first([this, index](edm::WaitingTaskHolder holder) mutable {
                  if (beginTransitionRan_[index.id()] == 1) {
                    processEndAsync(index, std::move(holder));
                  }
                }) |
                chain::then([this, index, resumer = std::move(resumer)](std::exception_ptr const* ptr,
                                                                        edm::WaitingTaskHolder holder) mutable {
                  releaseResources(index);
                  resumer.resume();
                  if (ptr) {
                    failureDuringProcessing_->store(true);
                    holder.doneWaiting(*ptr);
                  } else {
                    holder.doneWaiting(std::exception_ptr{});
                  }
                }) |
                chain::lastTask(std::move(holder));
    transitionResource_ = std::make_shared<ConcurrentTransitionResource>(transition_, index, recordID, std::move(task));
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newSupporterTransitionResource(transition_, transitionResource_);
    }
  }
  void ConcurrentTransitionScheduler::processBeginAsync(edm::ConcurrentTransitionID stream,
                                                        edm::WaitingTaskHolder holder) {
    // Simulate beginning the global transition here
    for (const auto& action : beginActions_) {
      action->performAsync(holder, transition_, stream, concurrentRecords_[stream.id()]);
    }
    //std::cout << "global begin transition " << transition_.name() << " for stream " << stream.id() << std::endl;
    holder.doneWaiting(std::exception_ptr{});
  }
  void ConcurrentTransitionScheduler::pauseAndEnqueueBeginSupporterTransitionAsync(
      edm::TransitionRecordKey const& key,
      edm::TransitionRecordID const& recordID,
      edm::WaitingTaskList& waitingTasks,
      edm::WaitingTaskHolder holder) {
    //we do a pause first so the tasks will not immediately be queued to TBB
    queue_.pauseAll();
    queue_.pushToAllAndPause(
        *holder.group(), [this, holder, key, recordID, &waitingTasks](auto iResumer, size_t index) mutable {
          using namespace edm::waiting_task::chain;
          beginSupporterTransitionRan_[index][key] = false;
          auto task = first([this, index, key, recordID](edm::WaitingTaskHolder holder) mutable {
                        processBeginSupporterTransitionAsync(
                            edm::ConcurrentTransitionID(index), key, recordID, std::move(holder));
                      }) |
                      then([this, index, resumer = std::move(iResumer)](std::exception_ptr const* ptr,
                                                                        edm::WaitingTaskHolder holder) mutable {
                        resumer.resume();
                        if (ptr) {
                          failureDuringProcessing_->store(true);
                          holder.doneWaiting(*ptr);
                        } else {
                          holder.doneWaiting(std::exception_ptr{});
                        }
                      }) |
                      lastTask(std::move(holder));
          waitingTasks.add(std::move(task));
        });
  }
  void ConcurrentTransitionScheduler::processBeginSupporterTransitionAsync(ConcurrentTransitionID stream,
                                                                           edm::TransitionRecordKey const& key,
                                                                           edm::TransitionRecordID const& recordID,
                                                                           edm::WaitingTaskHolder holder) {
    auto it = supporterBeginActions_.find(key);
    if (it == supporterBeginActions_.end()) {
      return;
    }
    beginSupporterTransitionRan_[stream.id()][key] = true;
    for (const auto& action : it->second) {
      action->performAsync(holder, key, stream, recordID);
    }
    // Simulate beginning the dependent transition here
    //std::cout << "begin dependent transition " << key.name() << " for stream " << stream.id() << std::endl;
  }

  void ConcurrentTransitionScheduler::endSupporterTransitionAsync(edm::TransitionRecordKey const& key,
                                                                  edm::TransitionRecordID const& recordID,
                                                                  edm::WaitingTaskHolder holder) {
    //only after all concurrent transitions have finished, so we can be sure that the resource is not being used anymore and we can safely reset it here.
    queue_.pushToAllAndPause(*holder.group(), [this, holder, key, recordID](auto iResumer, size_t index) mutable {
      using namespace edm::waiting_task::chain;
      first([this, index, key, recordID](edm::WaitingTaskHolder holder) mutable {
        processEndSupporterTransitionAsync(edm::ConcurrentTransitionID(index), key, recordID, std::move(holder));
      }) |
          then([this, index, resumer = std::move(iResumer)](std::exception_ptr const* ptr,
                                                            edm::WaitingTaskHolder holder) mutable {
            resumer.resume();
            if (ptr) {
              holder.doneWaiting(*ptr);
            } else {
              holder.doneWaiting(std::exception_ptr{});
            }
          }) |
          runLast(std::move(holder));
    });
    // Simulate ending the dependent transition here
    holder.doneWaiting(std::exception_ptr{});
  }

  void ConcurrentTransitionScheduler::processEndSupporterTransitionAsync(ConcurrentTransitionID stream,
                                                                         edm::TransitionRecordKey const& key,
                                                                         edm::TransitionRecordID const& recordID,
                                                                         edm::WaitingTaskHolder holder) {
    auto it = supporterEndActions_.find(key);
    if (it == supporterEndActions_.end()) {
      return;
    }
    if (!beginSupporterTransitionRan_[stream.id()][key]) {
      //if the begin transition didn't run then we shouldn't run the end transition
      return;
    }
    for (const auto& action : it->second) {
      action->performAsync(holder, key, stream, recordID);
    }
    // Simulate ending the dependent transition here
    holder.doneWaiting(std::exception_ptr{});
  }

  void ConcurrentTransitionScheduler::processEndAsync(edm::ConcurrentTransitionID stream,
                                                      edm::WaitingTaskHolder holder) {
    // Simulate ending the global transition here
    for (const auto& action : endActions_) {
      action->performAsync(holder, transition_, stream, concurrentRecords_[stream.id()]);
    }
    //std::cout << "global end transition " << transition_.name() << " for stream " << stream.id() << std::endl;
    holder.doneWaiting(std::exception_ptr{});
  }

}  // namespace edm
