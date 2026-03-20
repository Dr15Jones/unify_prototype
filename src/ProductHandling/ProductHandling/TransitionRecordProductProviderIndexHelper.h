#ifndef ProductHandling_TransitionRecordProductProviderIndexHelper_h
#define ProductHandling_TransitionRecordProductProviderIndexHelper_h

#include "DataModel/ProductKey.h"
#include "ProductHandling/TransitionProductProviderIndex.h"
#include "ProductHandling/ProductProviderBundleKey.h"

#include <vector>
#include <map>

namespace edm {
  class TransitionRecordProductProviderIndexHelper {
  public:
    ~TransitionRecordProductProviderIndexHelper() = default;
    void insert(ProductProviderBundleKey const& iProductProviderBundleKey, std::vector<ProductKey> const& productKeys);

    TransitionProductProviderIndex indexForProduct(ProductKey const& productKey) const;
    size_t numberOfProducts() const { return keyToIndex_.size(); }
    TransitionProductProviderIndex indexForProvider(ProductProviderBundleKey const& iProductProviderBundleKey, unsigned int providerIndex) const;

    unsigned int numberOfProviderIndices() const { return nextIndex_; }

  private:
    std::map<ProductKey, TransitionProductProviderIndex> keyToIndex_;
    std::map<ProductProviderBundleKey, std::vector<TransitionProductProviderIndex>> providersIndices_;
    unsigned int nextIndex_ = 0;
    
  };
}  // namespace edm

#endif