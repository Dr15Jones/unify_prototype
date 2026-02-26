#ifndef ProductHandling_TransitionProviderContext_h
#define ProductHandling_TransitionProviderContext_h
/* 
Holds the TransitionRecordImpl related to the present transition.
*/

#include "DataModel/TransitionRecordKey.h"
#include "ProductHandling/TransitionProductProviders.h"
#include <map>
namespace edm {
  class TransitionProviderContext {
  public:
    TransitionProviderContext() = default;

    void insert(TransitionProductProviders const& record) { providers_.insert(std::make_pair(record.key(), &record)); }

    TransitionProductProviders const* get(TransitionRecordKey const& key) const {
      auto it = providers_.find(key);
      if (it != providers_.end()) {
        return it->second;
      }
      return nullptr;
    }

  private:
    std::map<TransitionRecordKey, TransitionProductProviders const*> providers_;
  };
}  // namespace edm

#endif