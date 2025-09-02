#include "ProductHandling/ProductResolverIndexHelper.h"
#include "Base/Exception.h"

void edm::ProductResolverIndexHelper::insert(ProductKey const& productKey) {
  auto index = keyToIndex_.find(productKey);
  if (index != keyToIndex_.end()) {
    throw cms::Exception("ProductResolverIndexHelper")
        << "ProductKey already exists: " << productKey.typeID().name() << " " << productKey.moduleLabel() << " "
        << productKey.productInstanceName() << " " << productKey.processName();
  }
  ProductResolverIndex newIndex(keyToIndex_.size());
  keyToIndex_[productKey] = newIndex;
}

void edm::ProductResolverIndexHelper::finalize(std::vector<std::string_view> const& processNameOrder) {
  //For each element, check to see if this should be the entry for the empty process name, and if so add
  // the entry
  std::map<ProductKey, std::pair<std::string,ProductResolverIndex>> defaultLookup;
  for (auto const& keyAndIndex : keyToIndex_) {
    auto const& testKey = keyAndIndex.first;
    ProductKey newKey{testKey.typeID(), testKey.moduleLabel(), testKey.productInstanceName(), ""};
    auto& prod = defaultLookup[newKey];
    if (prod.first.empty()) {
      prod.first = testKey.processName();
      prod.second = keyAndIndex.second;
    } else {
      if (std::find(processNameOrder.begin(), processNameOrder.end(), testKey.processName()) <
          std::find(processNameOrder.begin(), processNameOrder.end(), prod.first)) {
        prod.first = testKey.processName();
        prod.second = keyAndIndex.second;
      }
    }
  }
  for(auto const& def: defaultLookup) {
    keyToIndex_[def.first] = def.second.second;
  }
}

edm::ProductResolverIndex edm::ProductResolverIndexHelper::getIndex(ProductKey const& productKey) const {
  auto index = keyToIndex_.find(productKey);
  if (index == keyToIndex_.end()) {
    return edm::ProductResolverIndex();  // Return an uninitialized index if not found
  }
  return index->second;
}
