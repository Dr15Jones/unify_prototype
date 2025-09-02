#include "ProductHandling/ProductResolverIndexHelpersBuilder.h"
#include "ProductHandling/ProductResolversProvider.h"
#include "DataModel/ProductKey.h"

void edm::ProductResolverIndexHelpersBuilder::determineProductsFrom(ProductResolversProvider const& provider) {
  auto resolverRecords = provider.resolverRecords();
  for (const auto& recordKey : resolverRecords) {
    std::vector<ProductKey> productKeys = provider.productKeysForRecord(recordKey);
    for (const auto& productKey : productKeys) {
      auto& helper = helpers_[recordKey];
      if (!helper) {
        helper = std::make_shared<ProductResolverIndexHelper>();
      }
      helper->insert(productKey);
    }
  }
}

void edm::ProductResolverIndexHelpersBuilder::finalize(std::vector<std::string_view> const& processHistoryOrder) {
  for(auto& helper: helpers_) {
    helper.second->finalize(processHistoryOrder);
  }
}

std::vector<edm::TransitionRecordKey> edm::ProductResolverIndexHelpersBuilder::usedRecords() const {
  std::vector<TransitionRecordKey> keys;
  keys.reserve(helpers_.size());
  for (const auto& pair : helpers_) {
    keys.push_back(pair.first);
  }
  return keys;
}

std::shared_ptr<edm::ProductResolverIndexHelper> edm::ProductResolverIndexHelpersBuilder::helperFor(
    TransitionRecordKey const& key) const {
  auto it = helpers_.find(key);
  if (it != helpers_.end()) {
    return it->second;
  }
  return nullptr;  // Return nullptr if the helper is not found
}
