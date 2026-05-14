#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/TransitionsDistributorGuard.h"
#include "TransitionHandling/FilesProcessor.h"
#include "TransitionHandling/AsyncActionBase.h"
#include "Concurrency/chain_first.h"

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
    std::shared_ptr<FileTransitionResource> fileResource = fileTransitionResource_.lock();
    assert(fileResource);
    std::unique_ptr<edm::ConcurrentTransitionID> activeStream = std::make_unique<edm::ConcurrentTransitionID>(std::numeric_limits<std::size_t>::max());
    auto* pActiveStream = activeStream.get();
    chain::first([&coordinator, recordID, this, pActiveStream, lastTask = distributor.finalTask()](
                     edm::WaitingTaskHolder holder) mutable {
      queue_.pushAndPause(
          *holder.group(),
          [this, &coordinator, recordID, pActiveStream, holder = std::move(holder), lastTask = std::move(lastTask)](
              auto iResumer, size_t index) mutable {
            edm::ConcurrentTransitionID streamID(index);
            streamRecords_[index] = recordID;
            *pActiveStream = streamID;
            announceNewTransitionComing(streamID, recordID, std::move(iResumer), std::move(lastTask));
            holdResources(streamID);
            coordinator.readTransitionAsync(transition_, streamID, std::move(holder));
          });
    }) |
        chain::then(
            [this, distributor = std::move(distributor), finalHolder = holder, stream = std::move(activeStream)](
                edm::WaitingTaskHolder holder) mutable {
              //we can release the distributor and allow the beginGlobalAsync to happen concurrently.
              distributor.release();
              // Simulate processing the transition here
              beginGlobalAsync(*stream, std::move(holder));
            }) |
        chain::then([fileResource = std::move(fileResource)](edm::WaitingTaskHolder holder) mutable {
          holder.doneWaiting(std::exception_ptr{});
        }) |
        chain::runLast(std::move(holder));
  }

  //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::newDataDependentTransitionComing(edm::TransitionRecordKey transitionKey,
                                        std::shared_ptr<ConcurrentTransitionResource> resource) {
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newDataDependentTransitionComing(transitionKey, resource);
    }
    dataDependentTransitions_[transitionKey] = std::move(resource);
  }

  void ConcurrentTransitionScheduler::newFileComing(std::weak_ptr<FileTransitionResource> resource) { fileTransitionResource_ = std::move(resource); }

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
      streamHeldResources_[id.id()][i] = resource;
      ++i;
    }
  }
  void ConcurrentTransitionScheduler::releaseResources(edm::ConcurrentTransitionID id) {
    for (auto& resource : streamHeldResources_[id.id()]) {
      resource.reset();
    }
  }
  //Called while the TransitionDistrobutor is still paused, so we don't have to worry about synchronization here.
  void ConcurrentTransitionScheduler::announceNewTransitionComing(edm::ConcurrentTransitionID index,
                                   edm::TransitionRecordID const& recordID,
                                   edm::IndexedLimitedTaskQueue::Resumer resumer,
                                   edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;
    auto task = chain::first([this, index](edm::WaitingTaskHolder holder) mutable {
                  endGlobalAsync(index, std::move(holder));
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
      scheduler->newDataDependentTransitionComing(transition_, transitionResource_);
    }
  }
  void ConcurrentTransitionScheduler::beginGlobalAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder) {
    // Simulate beginning the global transition here
    for (const auto& action : beginActions_) {
      action->performAsync(holder, transition_, stream, streamRecords_[stream.id()]);
    }
    //std::cout << "global begin transition " << transition_.name() << " for stream " << stream.id() << std::endl;
    holder.doneWaiting(std::exception_ptr{});
  }
  void ConcurrentTransitionScheduler::beginDependentTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskHolder holder) {
    // Simulate beginning the dependent transition here
    holder.doneWaiting(std::exception_ptr{});
  }
  void ConcurrentTransitionScheduler::endDependentTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskHolder holder) {
    // Simulate ending the dependent transition here
    holder.doneWaiting(std::exception_ptr{});
  }
  void ConcurrentTransitionScheduler::endGlobalAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder) {
    // Simulate ending the global transition here
    for (const auto& action : endActions_) {
      action->performAsync(holder, transition_, stream, streamRecords_[stream.id()]);
    }
    //std::cout << "global end transition " << transition_.name() << " for stream " << stream.id() << std::endl;
    holder.doneWaiting(std::exception_ptr{});
  }

}
