#ifndef ConditionsHandling_ConditionsIntervalFinder_h
#define ConditionsHandling_ConditionsIntervalFinder_h

#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "DataModel/TransitionRecordID.h"
#include "ConditionsHandling/ValidityInterval.h"

#include <vector>

namespace edm {
  class ConditionsIntervalFinder {
  public:
    virtual ~ConditionsIntervalFinder() = default;

    virtual std::vector<ConditionsRecordKey> findingForRecords() const = 0;
    virtual ValidityInterval findIntervalFor(ConditionsRecordKey const& iKey, edm::TransitionRecordID const& iTime) const = 0;
  };
}
#endif // ConditionsHandling_ConditionsIntervalFinder_h