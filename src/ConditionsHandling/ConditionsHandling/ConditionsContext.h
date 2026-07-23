#ifndef ConditionsHandling_ConditionsConext_h
#define ConditionsHandling_ConditionsConext_h
#include "ConditionsHandling/ValidityInterval.h"
#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "DataModel/TransitionRecordID.h"
#include <unordered_map>

namespace edm {
  class ConditionsContext {
  public:
    ConditionsContext() = default;
    ~ConditionsContext() = default;

  };
}  // namespace edm
#endif  // ConditionsHandling_ConditionsConext_h