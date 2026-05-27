#ifndef TransitionHandling_SourceCoordinator_h
#define TransitionHandling_SourceCoordinator_h
#include "DataModel/TransitionRecordKey.h"
#include "TransitionHandling/SourceBase.h"
#include "TransitionHandling/SourcePeekResult.h"
#include "TransitionHandling/ConcurrentTransitionID.h"
#include "TransitionHandling/TransitionRecordID.h"
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
        cachedNextTransition_ = source_->goToNextTransition();
      }
      result = *cachedNextTransition_;
      holder.doneWaiting(std::exception_ptr{});
    });
  }
  void readTransitionAsync(edm::TransitionRecordKey transitionKey, edm::ConcurrentTransitionID streamID, edm::WaitingTaskHolder holder) {
    queue_.push(*holder.group(), [this, transitionKey, streamID, holder = std::move(holder)]() mutable {
      assert(cachedNextTransition_);
      assert(cachedNextTransition_->recordKey() == transitionKey);
      source_->readTransition(transitionKey, streamID);
      // Simulate reading the transition here
      cachedNextTransition_.reset();
      holder.doneWaiting(std::exception_ptr{});
    });
  }
  void goToNextTransitionAsync(edm::WaitingTaskHolder holder) {
    queue_.push(*holder.group(), [this, holder = std::move(holder)]() mutable {
      cachedNextTransition_ = source_->goToNextTransition();
      holder.doneWaiting(std::exception_ptr{});
    });
  }

  void mergeTransitionAsync(edm::TransitionRecordKey transitionKey,
                            edm::TransitionRecordID const& recordID,
                            edm::ConcurrentTransitionID streamID,
                            edm::WaitingTaskHolder holder) {
    queue_.push(*holder.group(), [this, transitionKey, recordID, streamID, holder = std::move(holder)]() mutable {
      source_->mergeTransition(transitionKey, recordID, streamID);
      cachedNextTransition_.reset();
      holder.doneWaiting(std::exception_ptr{});
    });
  }

private:
  std::unique_ptr<edm::SourceBase> source_;
  std::optional<edm::SourcePeekResult> cachedNextTransition_;
  edm::SerialTaskQueue queue_;
};

}

#endif /* TransitionHandling_SourceCoordinator_h */