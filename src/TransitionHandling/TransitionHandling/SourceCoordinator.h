#ifndef TransitionHandling_SourceCoordinator_h
#define TransitionHandling_SourceCoordinator_h
#include "DataModel/TransitionRecordKey.h"
#include "TransitionHandling/SourceBase.h"
#include "TransitionHandling/SourcePeekResult.h"
#include "TransitionHandling/ConcurrentTransitionID.h"
#include "DataModel/TransitionRecordID.h"
#include "Concurrency/SerialTaskQueue.h"
#include "Concurrency/WaitingTaskHolder.h"

#include <memory>
#include <optional>
#include <cassert>

namespace edm {
  class SourceCoordinator {
  public:
    SourceCoordinator(std::unique_ptr<edm::SourceBase> source) : source_(std::move(source)) { assert(source_); }
    void peekNextTransitionAsync(std::optional<edm::SourcePeekResult>& result, edm::WaitingTaskHolder holder) {
      queue_.push(*holder.group(), [this, &result, holder = std::move(holder)]() mutable {
        if (not cachedNextTransition_) {
          try {
            cachedNextTransition_ = source_->goToNextTransition();
          } catch (...) {
            holder.doneWaiting(std::current_exception());
            return;
          }
        }
        result = *cachedNextTransition_;
        holder.doneWaiting(std::exception_ptr{});
      });
    }
    void readTransitionAsync(edm::TransitionRecordKey transitionKey,
                             edm::ConcurrentTransitionID streamID,
                             edm::WaitingTaskHolder holder) {
      queue_.push(*holder.group(), [this, transitionKey, streamID, holder = std::move(holder)]() mutable {
        assert(cachedNextTransition_);
        assert(cachedNextTransition_->recordKey() == transitionKey);
        auto const lastTransition = *cachedNextTransition_;
        cachedNextTransition_.reset();
        try {
          bool doMerge = false;
          source_->readTransition(transitionKey, streamID);
          do {
            auto nxt = source_->goToNextTransition();
            if (nxt.state() == edm::SourceNextState::DataTransition and
                lastTransition.recordKey() == nxt.recordKey() and *lastTransition.recordID() == *nxt.recordID()) {
              //we have a merge case, need to merge before we can read the transition
              doMerge = true;
              source_->mergeTransition(transitionKey, *nxt.recordID(), streamID);
            } else {
              doMerge = false;
              cachedNextTransition_ = nxt;
            }
          } while (doMerge);
        } catch (...) {
          holder.doneWaiting(std::current_exception());
          return;
        }
        // Simulate reading the transition here
        holder.doneWaiting(std::exception_ptr{});
      });
    }
    void goToNextTransitionAsync(edm::WaitingTaskHolder holder) {
      queue_.push(*holder.group(), [this, holder = std::move(holder)]() mutable {
        try {
          cachedNextTransition_ = source_->goToNextTransition();
        } catch (...) {
          holder.doneWaiting(std::current_exception());
          return;
        }
        holder.doneWaiting(std::exception_ptr{});
      });
    }

    void mergeTransitionAsync(edm::TransitionRecordKey transitionKey,
                              edm::TransitionRecordID const& recordID,
                              edm::ConcurrentTransitionID streamID,
                              edm::WaitingTaskHolder holder) {
      queue_.push(*holder.group(), [this, transitionKey, recordID, streamID, holder = std::move(holder)]() mutable {
        cachedNextTransition_.reset();
        try {
          source_->mergeTransition(transitionKey, recordID, streamID);
        } catch (...) {
          holder.doneWaiting(std::current_exception());
          return;
        }
        holder.doneWaiting(std::exception_ptr{});
      });
    }

  private:
    std::unique_ptr<edm::SourceBase> source_;
    std::optional<edm::SourcePeekResult> cachedNextTransition_;
    edm::SerialTaskQueue queue_;
  };

}  // namespace edm

#endif /* TransitionHandling_SourceCoordinator_h */