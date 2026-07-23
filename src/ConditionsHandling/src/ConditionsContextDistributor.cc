#include "ConditionsHandling/ConditionsContextDistributor.h"
#include "ConditionsHandling/ConcurrentIntervalScheduler.h"
#include <iostream>
namespace edm {
  void ConditionsContextDistributor::contextForAsync(TransitionRecordID const& recordID,
                                                     ConditionsContextResource& iContext,
                                                     edm::WaitingTaskHolder holder) {
    for (auto& [key, scheduler] : recordKeyToScheduler_) {
      auto& interval = recordKeyToLatestInterval_[key];
      if (not interval.validFor(recordID)) {
        interval = intervalCoordinator_.findIntervalFor(key, recordID);
        recordKeyToLatestInterval_[key] = interval;
      }
      scheduler->scheduleIntervalAsync(interval, iContext, holder);
    }
  }
  void ConditionsContextDistributor::releaseCurrentResources() {
    for (auto& pair : recordKeyToScheduler_) {
      pair.second->releaseCurrentResource();
    }
  }

}  // namespace edm