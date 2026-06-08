#include "TransitionHandling/TransitionsDistributor.h"
#include "TransitionHandling/TransitionsDistributorGuard.h"
#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "Concurrency/chain_first.h"
#include "Concurrency/FinalWaitingTask.h"

namespace edm {

  void TransitionsDistributor::addSchedulerForTransition(edm::TransitionRecordKey transitionKey,
                                                         ConcurrentTransitionScheduler& scheduler) {
    filesProcessor_.addDependentScheduler(scheduler);
    scheduler.setFailureDuringProcessing(&failureDuringProcessing_);
    schedulers_.emplace(transitionKey, &scheduler);
  }

  //peeks at the source and then sends the transition to the proper StreamScheduler

  void TransitionsDistributor::processData() {
    oneapi::tbb::task_group group;
    edm::FinalWaitingTask finalTask(group);
    edm::WaitingTaskHolder holder(group, &finalTask);
    distributeNextTransitionAsync(std::move(holder));
    finalTask.wait();
  }

  void TransitionsDistributor::distributeNextTransitionAsync(edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;
    chain::first([this](edm::WaitingTaskHolder holder) {
      if (failureDuringRead_ or failureDuringProcessing_) {
        holder.doneWaiting(std::exception_ptr{});
        return;
      }
      coordinator_.peekNextTransitionAsync(nextTransition_, std::move(holder));
    }) | chain::then([this, finalTask = holder](std::exception_ptr const* ptr, edm::WaitingTaskHolder holder) {
      /*if (nextTransition_) {
        std::string recordKeyStr = nextTransition_->recordKey() ? nextTransition_->recordKey()->name() : "";
        //std::cout << " New transition peeked: " << static_cast<int>(nextTransition_->state()) <<" " << recordKeyStr << std::endl;
      } else {
        //std::cout << " No transition peeked" << std::endl;
      }
        */

      if (ptr or failureDuringRead_ or failureDuringProcessing_ or (not nextTransition_.has_value()) or
          nextTransition_.value().state() == edm::SourceNextState::Stop) {
        // No more transitions to process, just finish
        filesProcessor_.doneProcessing();
        for (auto& [_, scheduler] : schedulers_) {
          scheduler->doneProcessing();
        }
        if(ptr) {
          holder.doneWaiting(*ptr);
        } else {
          holder.doneWaiting(std::exception_ptr{});
        }
        return;
      }
      if (nextTransition_.value().state() == edm::SourceNextState::File) {
        // Handle file transition separately since it doesn't go through the StreamSchedulers
        chain::first([this, finalTask = finalTask](edm::WaitingTaskHolder holder) mutable {
          filesProcessor_.processFileTransitionAsync(coordinator_, finalTask, std::move(holder));
        }) | chain::then([this](edm::WaitingTaskHolder holder) mutable {
          tryToMergeAfterNewFileAsync(std::nullopt, std::move(holder));
        }) | chain::then([this, finalTask](std::exception_ptr const* ptr, edm::WaitingTaskHolder holder) mutable {
          if (ptr) {
            failureDuringRead_ = true;
            auto tmp = finalTask;
            tmp.doneWaiting(*ptr);
          }
          distributeNextTransitionAsync(std::move(finalTask));
        }) | chain::runLast(std::move(holder));
        return;
      }
      assert(nextTransition_.has_value());
      assert(nextTransition_.value().recordKey());
      auto scheduler = schedulers_.find(*nextTransition_.value().recordKey());
      assert(scheduler != schedulers_.end());
      scheduler->second->readAsync(coordinator_,
                                   nextTransition_.value().recordID().value(),
                                   TransitionsDistributorGuard(*this, std::move(finalTask)),
                                   std::move(holder));
    }) | chain::runLast(std::move(holder));
  }

  void TransitionsDistributor::tryToMergeAfterNewFileAsync(std::optional<edm::TransitionRecordID> recordID,
                                                           edm::WaitingTaskHolder holder) {
    using namespace edm::waiting_task;
    chain::first([this](edm::WaitingTaskHolder holder) mutable {
      coordinator_.peekNextTransitionAsync(nextTransition_, std::move(holder));
    }) | chain::then([this, recordID, finalTask = holder](edm::WaitingTaskHolder holder) mutable {
      // Check if we can merge any transitions after a new file comes in
      if (!nextTransition_ || nextTransition_.value().state() != edm::SourceNextState::DataTransition) {
        holder.doneWaiting(std::exception_ptr{});
        return;
      }
      if (recordID && nextTransition_->recordID() == *recordID) {
        // the transition didn't change so the previous merge must not have happened
        holder.doneWaiting(std::exception_ptr{});
        return;
      }
      assert(nextTransition_.value().recordKey());
      auto found = schedulers_.find(*nextTransition_.value().recordKey());
      assert(found != schedulers_.end());

      auto* scheduler = found->second;
      chain::first([this, scheduler = scheduler, finalTask = std::move(finalTask)](
                       edm::WaitingTaskHolder holder) mutable {
        if (nextTransition_ && nextTransition_->recordID()) {
          scheduler->tryToMergeAsync(coordinator_, nextTransition_->recordID().value(), std::move(holder));
        }
      }) | chain::then([this](edm::WaitingTaskHolder holder) mutable {
        // After trying to merge, we should check if the transition changed and if we can merge another one
        if (nextTransition_ && nextTransition_->state() == edm::SourceNextState::DataTransition) {
          assert(nextTransition_->recordID());
          tryToMergeAfterNewFileAsync(nextTransition_->recordID(), std::move(holder));
        }
      }) | chain::runLast(std::move(holder));
    }) | chain::runLast(std::move(holder));
  }

}  // namespace edm