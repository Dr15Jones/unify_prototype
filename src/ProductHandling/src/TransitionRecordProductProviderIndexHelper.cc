#include "ProductHandling/TransitionRecordProductProviderIndexHelper.h"
#include "Base/Exception.h"
#include <cassert>

namespace edm {
  void TransitionRecordProductProviderIndexHelper::insert(ProductProviderBundleKey const& iProductProviderBundleKey, std::vector<ProductKey> const& productKeys) {
    const unsigned int indexToUse = nextIndex_++;
    providersIndices_[iProductProviderBundleKey].emplace_back(indexToUse);
    for (auto const& key : productKeys) {
      auto index = keyToIndex_.find(key);
      if (index != keyToIndex_.end()) {
        throw cms::Exception("TransitionRecordProductProviderIndexHelper")
            << "ProductKey already exists: " << key.typeID().name() << " " << key.moduleLabel() << " "
            << key.productInstanceName() << " " << key.processName();
      }
      TransitionProductProviderIndex newIndex(indexToUse);
      keyToIndex_[key] = newIndex;
    }
  }

  TransitionProductProviderIndex TransitionRecordProductProviderIndexHelper::indexForProduct(
      ProductKey const& productKey) const {
    auto index = keyToIndex_.find(productKey);
    if (index == keyToIndex_.end()) {
      return TransitionProductProviderIndex();  // Return an uninitialized index if not found
    }
    return index->second;
  }

  TransitionProductProviderIndex TransitionRecordProductProviderIndexHelper::indexForProvider(ProductProviderBundleKey const& iProductProviderBundleKey, unsigned int providerIndex) const {
    auto it = providersIndices_.find(iProductProviderBundleKey);
    if (it == providersIndices_.end() || providerIndex >= it->second.size()) {
      return TransitionProductProviderIndex();  // Return an uninitialized index if not found
    }
    assert(providerIndex < it->second.size());
    return it->second[providerIndex];
  }
}  // namespace edm