#include "ConditionsHandling/ConditionsIntervalCoordinator.h"

namespace edm {
  ValidityInterval ConditionsIntervalCoordinator::findIntervalFor(
      edm::ConditionsRecordKey const& key, edm::TransitionRecordID const& recordID) const {
    auto it = recordKeyToFinder_.find(key);
    assert(it != recordKeyToFinder_.end());
    return it->second->findIntervalFor(key, recordID);
  }

}  // namespace edm