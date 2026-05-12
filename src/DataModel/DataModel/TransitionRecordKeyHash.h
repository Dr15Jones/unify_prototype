#ifndef DataModel_TransitionRecordKeyHash_h
#define DataModel_TransitionRecordKeyHash_h

#include "DataModel/TransitionRecordKey.h"

namespace edm {
  struct TransitionRecordKeyHash {
    auto operator()(edm::TransitionRecordKey const& key) const noexcept { return key.typeID().hash_code(); }
  };
}  // namespace edm
#endif  // DataModel_TransitionRecordKeyHash_h