#ifndef ConditionsHandling_ConditionsContextDistributor_h
#define ConditionsHandling_ConditionsContextDistributor_h

#include "ConditionsHandling/ConditionsIntervalCoordinator.h"
#include "ConditionsDataModel/ConditionsRecordKeyHash.h"
#include "ConditionsHandling/ConditionsContext.h"
#include "ConditionsHandling/ValidityInterval.h"
#include "DataModel/TransitionRecordID.h"
#include "Concurrency/WaitingTaskHolder.h"

#include <memory>
#include <unordered_map>

namespace edm {
  class ConditionsContextResource;
  class ConcurrentIntervalScheduler;

  class ConditionsContextDistributor {
  public:
    ConditionsContextDistributor() = default;
    ~ConditionsContextDistributor() = default;

    void addIntervalFinder(std::unique_ptr<ConditionsIntervalFinder> finder) {
      intervalCoordinator_.add(std::move(finder));
    }

    void addSchedulerForRecord(ConditionsRecordKey key, ConcurrentIntervalScheduler* scheduler) {
      recordKeyToScheduler_.emplace(key, scheduler);
      recordKeyToLatestInterval_.emplace(key, ValidityInterval());
    }

    std::vector<ConditionsRecordKey> availableRecords() const { return intervalCoordinator_.availableRecords(); }

    void contextForAsync(TransitionRecordID const& recordID,
                         ConditionsContextResource& iContext,
                         edm::WaitingTaskHolder holder);

    void releaseCurrentResources();
   
  private:
    ConditionsIntervalCoordinator intervalCoordinator_;
    std::unordered_map<ConditionsRecordKey, ConcurrentIntervalScheduler*, ConditionsRecordKeyHash> recordKeyToScheduler_;
    std::unordered_map<ConditionsRecordKey, ValidityInterval, ConditionsRecordKeyHash> recordKeyToLatestInterval_;
  };
}  // namespace edm

#endif