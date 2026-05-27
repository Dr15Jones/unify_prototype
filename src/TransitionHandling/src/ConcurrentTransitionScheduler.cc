#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/TransitionsDistributorGuard.h"
#include "TransitionHandling/FilesProcessor.h"
#include "TransitionHandling/AsyncActionBase.h"
#include "Concurrency/chain_first.h"
#include "Concurrency/WaitingTaskList.h"

namespace edm {
  //Must only be called by TransitionsDistributor (as it serializes the calls to readAsync and tryToMergeAsync)
  void ConcurrentTransitionScheduler::doneProcessing() {
    transitionResource_.reset();
    for (auto& [_, resource] : dataDependentTransitions_) {
      resource.reset();
    }
  }
  void ConcurrentTransitionScheduler::readAsync(edm::SourceCoordinator& coordinator,
                                                edm::TransitionRecordID const& recordID,
                                                TransitionsDistributorGuard&& distributor,
                                                edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;
    transitionResource_.reset();
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionComing(transition_);
    }

    std::shared_ptr<FileTransitionResource> fileResource = fileTransitionResource_.lock();
    assert(fileResource);
    std::unique_ptr<edm::ConcurrentTransitionID> activeStream =
        std::make_unique<edm::ConcurrentTransitionID>(std::numeric_limits<std::size_t>::max());
    auto* pActiveStream = activeStream.get();
    chain::first([&coordinator, recordID, this, pActiveStream, lastTask = distributor.finalTask()](
                     edm::WaitingTaskHolder holder) mutable {
      queue_.pushAndPause(
          *holder.group(),
          [this, &coordinator, recordID, pActiveStream, holder = std::move(holder), lastTask = std::move(lastTask)](
              auto iResumer, size_t index) mutable {
            edm::ConcurrentTransitionID streamID(index);
            concurrentRecords_[index] = recordID;
            *pActiveStream = streamID;
            announceNewTransitionComing(streamID, recordID, std::move(iResumer), std::move(lastTask));
            holdResources(streamID);
            coordinator.readTransitionAsync(transition_, streamID, std::move(holder));
          });
    }) |
        chain::then([this, distributor = std::move(distributor), finalHolder = holder, pActiveStream](
                        edm::WaitingTaskHolder holder) mutable {
          //we can release the distributor and allow the beginGlobalAsync to happen concurrently.
          announceNewTransitionAvailable(*pActiveStream, finalHolder);
          distributor.release();
          // Simulate processing the transition here
          processBeginAsync(*pActiveStream, std::move(holder));
        }) |
        chain::then([this, fileResource = std::move(fileResource), stream = std::move(activeStream)](
                        edm::WaitingTaskHolder holder) mutable {
          // tell any waiting dependent transitions that the record is now available
          waitingDependentTransitionTasks_[stream->id()].doneWaiting(std::exception_ptr{});
          holder.doneWaiting(std::exception_ptr{});
        }) |
        chain::runLast(std::move(holder));
  }

  void ConcurrentTransitionScheduler::newDataDependentTransitionComing(edm::TransitionRecordKey transitionKey) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionComing(transitionKey);
    }
    auto findIt = dataDependentTransitions_.find(transitionKey);
    if (findIt == dataDependentTransitions_.end()) {
      throw std::runtime_error("Transition " + transitionKey.name() +
                               " is not already a data dependent transition of " + transition_.name());
    }
    dataDependentTransitions_[transitionKey].reset();
    transitionResource_.reset();
  }

  //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::newDataDependentTransitionResource(
      edm::TransitionRecordKey transitionKey, std::shared_ptr<ConcurrentTransitionResource> resource) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionResource(transitionKey, resource);
    }
    //if a new dependent is coming than this resource needs to be closed out
    transitionResource_.reset();
    auto findIt = dataDependentTransitions_.find(transitionKey);
    if (findIt == dataDependentTransitions_.end()) {
      throw std::runtime_error("Transition " + transitionKey.name() +
                               " is not already a data dependent transition of " + transition_.name());
    }
    dataDependentTransitions_[transitionKey] = std::move(resource);
    auto task = edm::waiting_task::chain::first(
                    [this, transitionKey, recordID = dataDependentTransitions_[transitionKey]->recordID_](
                        edm::WaitingTaskHolder holder) mutable {
                      endDependentTransitionAsync(transitionKey, recordID, std::move(holder));
                    }) |
                edm::waiting_task::chain::lastTask(dataDependentTransitions_[transitionKey]->holder_);
    dataDependentTransitions_[transitionKey]->holder_ = std::move(task);
  }

  void ConcurrentTransitionScheduler::announceNewTransitionAvailable(edm::ConcurrentTransitionID index,
                                                                     edm::WaitingTaskHolder holder) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionAvailable(transition_, waitingDependentTransitionTasks_[index.id()], holder);
    }
  }

  void ConcurrentTransitionScheduler::newDataDependentTransitionAvailable(edm::TransitionRecordKey transitionKey,
                                                                          edm::WaitingTaskList& waitingTasks,
                                                                          edm::WaitingTaskHolder holder) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionAvailable(transitionKey, waitingTasks, holder);
    }
    auto resource = dataDependentTransitions_[transitionKey];
    auto task =
        edm::waiting_task::chain::first([this, transitionKey, &waitingTasks](edm::WaitingTaskHolder holder) mutable {
          beginDependentTransitionAsync(transitionKey, waitingTasks, std::move(holder));
        }) |
        edm::waiting_task::chain::then([this, resource](edm::WaitingTaskHolder holder) mutable {
          //need to be sure that the resource is available during beginDependentTransitionAsync, but we can release it right after
          resource.reset();
        }) |
        edm::waiting_task::chain::lastTask(std::move(holder));
    waitingTasks.add(std::move(task));
  }

  void ConcurrentTransitionScheduler::newFileComing(std::weak_ptr<FileTransitionResource> resource) {
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
    for (auto& [_, resource] : dataDependentTransitions_) {
      concurrentHeldResources_[id.id()][i] = resource;
      ++i;
    }
  }
  void ConcurrentTransitionScheduler::releaseResources(edm::ConcurrentTransitionID id) {
    for (auto& resource : concurrentHeldResources_[id.id()]) {
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
                  processEndAsync(index, std::move(holder));
                }) |
                chain::then([this, index, resumer = std::move(resumer)](std::exception_ptr const* ptr,
                                                                        edm::WaitingTaskHolder holder) mutable {
                  releaseResources(index);
                  resumer.resume();
                  if (ptr) {
                    holder.doneWaiting(*ptr);
                  } else {
                    holder.doneWaiting(std::exception_ptr{});
                  }
                }) |
                chain::lastTask(std::move(holder));
    transitionResource_ = std::make_shared<ConcurrentTransitionResource>(index, recordID, std::move(task));
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionResource(transition_, transitionResource_);
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
  void ConcurrentTransitionScheduler::beginDependentTransitionAsync(edm::TransitionRecordKey key,
                                                                    edm::WaitingTaskList& waitingTasks,
                                                                    edm::WaitingTaskHolder holder) {
    queue_.pushToAllAndPause(*holder.group(), [this, holder, key, &waitingTasks](auto iResumer, size_t index) mutable {
      using namespace edm::waiting_task::chain;
      auto task = first([this, index, key](edm::WaitingTaskHolder holder) mutable {
                    processBeginDependentTransitionAsync(edm::ConcurrentTransitionID(index), key, std::move(holder));
                  }) |
                  then([this, index, resumer = std::move(iResumer)](edm::WaitingTaskHolder holder) mutable {
                    resumer.resume();
                  }) |
                  lastTask(std::move(holder));
      waitingTasks.add(std::move(task));
    });
    // Simulate beginning the dependent transition here
    holder.doneWaiting(std::exception_ptr{});
  }
  void ConcurrentTransitionScheduler::processBeginDependentTransitionAsync(ConcurrentTransitionID stream,
                                                                           edm::TransitionRecordKey const& key,
                                                                           edm::WaitingTaskHolder holder) {
    auto it = dependentBeginActions_.find(key);
    if (it == dependentBeginActions_.end()) {
      return;
    }
    for (const auto& action : it->second) {
      action->performAsync(holder, key, stream, dataDependentTransitions_[key]->recordID_);
    }
    // Simulate beginning the dependent transition here
    //std::cout << "begin dependent transition " << key.name() << " for stream " << stream.id() << std::endl;
  }

  void ConcurrentTransitionScheduler::endDependentTransitionAsync(edm::TransitionRecordKey const& key,
                                                                  edm::TransitionRecordID const& recordID,
                                                                  edm::WaitingTaskHolder holder) {
    //only after all concurrent transitions have finished, so we can be sure that the resource is not being used anymore and we can safely reset it here.
    queue_.pushToAllAndPause(*holder.group(), [this, holder, key, recordID](auto iResumer, size_t index) mutable {
      using namespace edm::waiting_task::chain;
      first([this, index, key, recordID](edm::WaitingTaskHolder holder) mutable {
        processEndDependentTransitionAsync(edm::ConcurrentTransitionID(index), key, recordID, std::move(holder));
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

  void ConcurrentTransitionScheduler::processEndDependentTransitionAsync(ConcurrentTransitionID stream,
                                                                         edm::TransitionRecordKey const& key,
                                                                         edm::TransitionRecordID const& recordID,
                                                                         edm::WaitingTaskHolder holder) {
    auto it = dependentEndActions_.find(key);
    if (it == dependentEndActions_.end()) {
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
