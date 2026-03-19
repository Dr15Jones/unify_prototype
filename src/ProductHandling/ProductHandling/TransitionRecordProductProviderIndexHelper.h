#ifndef ProductHandling_TransitionRecordProductProviderIndexHelper_h
#define ProductHandling_TransitionRecordProductProviderIndexHelper_h

#include "DataModel/ProductKey.h"
#include "ProductHandling/TransitionProductProviderIndex.h"
#include "ProductHandling/ProvidersKey.h"

#include <vector>
#include <map>

namespace edm {
  class TransitionRecordProductProviderIndexHelper {
  public:
    ~TransitionRecordProductProviderIndexHelper() = default;
    void insert(ProvidersKey const& iProvidersKey, std::vector<ProductKey> const& productKeys);

    TransitionProductProviderIndex indexForProduct(ProductKey const& productKey) const;
    size_t numberOfProducts() const { return keyToIndex_.size(); }
    TransitionProductProviderIndex indexForProvider(ProvidersKey const& iProvidersKey, unsigned int providerIndex) const;

    unsigned int numberOfProviderIndices() const { return nextIndex_; }

  private:
    std::map<ProductKey, TransitionProductProviderIndex> keyToIndex_;
    std::map<ProvidersKey, std::vector<TransitionProductProviderIndex>> providersIndices_;
    unsigned int nextIndex_ = 0;
    
  };
}  // namespace edm

#endif