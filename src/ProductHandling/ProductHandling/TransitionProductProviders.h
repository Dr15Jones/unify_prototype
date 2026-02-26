#ifndef ProductHandling_TransitionProductProviders_h
#define ProductHandling_TransitionProductProviders_h

#include "DataModel/TransitionRecordKey.h"
#include "ProductHandling/ProductProviderBase.h"
#include "ProductHandling/TransitionProductProviderIndex.h"

#include <vector>
namespace edm {
  class TransitionProductProviders {
  public:
    TransitionProductProviders(TransitionRecordKey key, std::vector<edm::ProductProviderBase*> providers)
        : key_(key), providers_(providers) {}
    TransitionRecordKey const& key() const { return key_; }

    TransitionProductProviderIndex indexForProvider(ProductProviderBase const* provider) const {
      for (size_t i = 0; i < providers_.size(); ++i) {
        if (providers_[i] == provider) {
          return TransitionProductProviderIndex(i);
        }
      }
      return TransitionProductProviderIndex();
    }
    edm::ProductProviderBase* providerForIndex(TransitionProductProviderIndex index) const {
      if (index.isUninitialized() || index.value() >= providers_.size()) {
        return nullptr;
      }
      return providers_[index.value()];
    }

  private:
    TransitionRecordKey key_;
    std::vector<edm::ProductProviderBase*> providers_;
  };
}  // namespace edm

#endif