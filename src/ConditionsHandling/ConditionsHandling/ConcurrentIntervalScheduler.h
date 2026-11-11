#ifndef ConditionsHandling_ConcurrentIntervalScheduler_h
#define ConditionsHandling_ConcurrentIntervalScheduler_h
/* 
 * A class for managing concurrent intervals in the conditions system for one Record type.
 */
#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "ConditionsHandling/ConcurrentIntervalID.h"
#include "ConditionsHandling/ConditionsContextResource.h"
#include "ConditionsHandling/ValidityInterval.h"
#include "Concurrency/IndexedLimitedTaskQueue.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ConditionsHandling/ConditionsRecordResource.h"

#include <memory>

namespace edm {
  class ConcurrentIntervalScheduler {
  public:
    ConcurrentIntervalScheduler(edm::ConditionsRecordKey const& iKey, unsigned int maxConcurrentIntervals)
        : recordKey_(iKey), queue_(maxConcurrentIntervals) {}

    void scheduleIntervalAsync(edm::ValidityInterval const& interval,
                               ConditionsContextResource& contextResource,
                               WaitingTaskHolder holder);

    void releaseCurrentResource() { currentResource_.reset(); }
      
  private:
    edm::ConditionsRecordKey recordKey_;
    edm::IndexedLimitedTaskQueue queue_;
    std::shared_ptr<edm::ConditionsRecordResource> currentResource_;

  };
}  // namespace edm
#endif