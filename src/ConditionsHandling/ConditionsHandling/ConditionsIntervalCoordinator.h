#ifndef ConditionsHandling_ConditionsIntervalCoordinator_h
#define ConditionsHandling_ConditionsIntervalCoordinator_h

/**
 * A class for coordinating the finding of valid intervals for conditions records.
 * This class manages multiple ConditionsIntervalFinder instances and provides a unified interface for finding valid intervals.
 */
#include <vector>
#include <memory>
#include <unordered_map>
#include "ConditionsHandling/ConditionsIntervalFinder.h"
#include "ConditionsDataModel/ConditionsRecordKeyHash.h"

namespace edm {
  class ConditionsIntervalCoordinator {
  public:
    ConditionsIntervalCoordinator() = default;

    void add(std::unique_ptr<ConditionsIntervalFinder> finder) {
      finders_.push_back(std::move(finder));
      for (auto const& key : finders_.back()->findingForRecords()) {
        recordKeyToFinder_.emplace(key, finders_.back().get());
      }
    }

    std::vector<ConditionsRecordKey> availableRecords() const {
      std::vector<ConditionsRecordKey> keys;
      for (auto const& [key, _] : recordKeyToFinder_) {
        keys.push_back(key);
      }
      return keys;
    }

    ValidityInterval findIntervalFor(edm::ConditionsRecordKey const& key,
                                     edm::TransitionRecordID const& recordID) const;

  private:
    std::unordered_map<ConditionsRecordKey, ConditionsIntervalFinder const*, ConditionsRecordKeyHash> recordKeyToFinder_;
    std::vector<std::unique_ptr<ConditionsIntervalFinder>> finders_;
  };
}  // namespace edm

#endif  // ConditionsHandling_ConditionsIntervalCoordinator_h