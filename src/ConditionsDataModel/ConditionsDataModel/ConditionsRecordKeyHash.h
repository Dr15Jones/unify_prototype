#ifndef ConditionsDataModel_ConditionsRecordKeyHash_h
#define ConditionsDataModel_ConditionsRecordKeyHash_h

#include "ConditionsDataModel/ConditionsRecordKey.h"

namespace edm {
  struct ConditionsRecordKeyHash {
    auto operator()(edm::ConditionsRecordKey const& key) const noexcept { return key.typeID().hash_code(); }
  };
}  // namespace edm
#endif  // ConditionsDataModel_ConditionsRecordKeyHash_h