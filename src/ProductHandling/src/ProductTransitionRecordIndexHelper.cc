#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
#include "Base/Exception.h"

void edm::ProductTransitionRecordIndexHelper::insert(ProductKey const& productKey) {
  auto index = keyToIndex_.find(productKey);
  if (index != keyToIndex_.end()) {
    throw cms::Exception("ProductTransitionRecordIndexHelper")
        << "ProductKey already exists: " << productKey.typeID().name() << " " << productKey.moduleLabel() << " "
        << productKey.productInstanceName() << " " << productKey.processName();
  }
  ProductTransitionRecordIndex newIndex(keyToIndex_.size());
  keyToIndex_[productKey] = newIndex;
}

void edm::ProductTransitionRecordIndexHelper::finalize(std::vector<std::string_view> const& processNameOrder) {
  //For each element, check to see if this should be the entry for the empty process name, and if so add
  // the entry
  std::map<ProductKey, std::pair<std::string,ProductTransitionRecordIndex>> defaultLookup;
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

edm::ProductTransitionRecordIndex edm::ProductTransitionRecordIndexHelper::getIndex(ProductKey const& productKey) const {
  auto index = keyToIndex_.find(productKey);
  if (index == keyToIndex_.end()) {
    return edm::ProductTransitionRecordIndex();  // Return an uninitialized index if not found
  }
  return index->second;
}
