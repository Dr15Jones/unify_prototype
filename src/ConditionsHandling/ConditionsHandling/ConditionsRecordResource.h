#ifndef ConditionsHandling_CondidtionsContextRecordResource_h
#define ConditionsHandling_CondidtionsContextRecordResource_h
#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "ConditionsHandling/ValidityInterval.h"
#include "ConditionsHandling/ConcurrentIntervalID.h"
#include "Concurrency/IndexedLimitedTaskQueue.h"
#include <optional>

namespace edm {
  class ConditionsRecordResource {
  public:
    explicit ConditionsRecordResource(ConditionsRecordKey key, ConcurrentIntervalID id, ValidityInterval interval)
        : recordKey_(key), recordID_(id), validityInterval_(interval) {}
    ~ConditionsRecordResource() = default;

    ConditionsRecordKey recordKey_;
    ConcurrentIntervalID recordID_;
    ValidityInterval validityInterval_;
    std::optional<edm::IndexedLimitedTaskQueue::Resumer> resumer_;
  };
}  // namespace edm
#endif  // ConditionsHandling_CondidtionsContextRecordResource_h