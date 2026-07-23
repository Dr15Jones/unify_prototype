#include "ConditionsHandling/ConcurrentIntervalScheduler.h"
#include <iostream>
namespace edm {
  void ConcurrentIntervalScheduler::scheduleIntervalAsync(edm::ValidityInterval const& interval,
                                                          ConditionsContextResource& contextResource,
                                                          WaitingTaskHolder holder) {
    if (interval.begin().size() == 0) {
      //no valid interval for this record, so we don't need to schedule anything
      currentResource_.reset();
      return;
    }
    if (currentResource_ && currentResource_->validityInterval_.begin() == interval.begin()) {
      currentResource_->validityInterval_ = interval;
      contextResource.addRecordResource(currentResource_);
      return;
    }
    //we are getting a new current, so release the old as we may need the resource it holds to be available for the new interval
    currentResource_.reset();
    queue_.pushAndPause(*holder.group(),
                        [this, &contextResource, holder, &interval](edm::IndexedLimitedTaskQueue::Resumer&& resumer,
                                                                    size_t index) mutable {
                          auto resource = std::make_shared<edm::ConditionsRecordResource>(
                              recordKey_, edm::ConcurrentIntervalID{index}, interval);
                          contextResource.addRecordResource(resource);
                          currentResource_ = std::move(resource);
                          currentResource_->resumer_ = std::make_optional(std::move(resumer));
                        });
  }

}  // namespace edm