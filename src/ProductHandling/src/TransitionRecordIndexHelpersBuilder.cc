#include "ProductHandling/TransitionRecordIndexHelpersBuilder.h"
#include "ProductHandling/ProductsProvider.h"
#include "DataModel/ProductKey.h"
#include "Base/Exception.h"

void edm::TransitionRecordIndexHelpersBuilder::determineProductsFrom(ProvidersKey const& key, ProductsProvider const& provider) {
  auto resolverRecords = provider.resolverRecords();
  for (const auto& recordKey : resolverRecords) {
    auto& providerHelper = providerHelpers_[recordKey];
    if (!providerHelper) {
      providerHelper = std::make_shared<ProviderTransitionRecordIndexHelper>();
    }
    auto& helper = helpers_[recordKey];
    if (!helper) {
      helper = std::make_shared<ProductTransitionRecordIndexHelper>();
    }

    auto numberOfProviders = provider.numberOfProvidersForRecord(recordKey);
    for (unsigned int providerIndex = 0; providerIndex < numberOfProviders; ++providerIndex) {
      auto productKeys = provider.productsFromProvider(recordKey, providerIndex);
      providerHelper->insert(key, productKeys);
      for (const auto& productKey : productKeys) {
        helper->insert(productKey);
      }
    }
  }
}

void edm::TransitionRecordIndexHelpersBuilder::finalize(std::vector<std::string_view> const& processHistoryOrder) {
  for (auto& helper : helpers_) {
    helper.second->finalize(processHistoryOrder);
  }
}

std::vector<edm::TransitionRecordKey> edm::TransitionRecordIndexHelpersBuilder::usedRecords() const {
  std::vector<TransitionRecordKey> keys;
  keys.reserve(helpers_.size());
  for (const auto& pair : helpers_) {
    keys.push_back(pair.first);
  }
  return keys;
}

std::shared_ptr<edm::ProductTransitionRecordIndexHelper> edm::TransitionRecordIndexHelpersBuilder::helperFor(
    TransitionRecordKey const& key) const {
  auto it = helpers_.find(key);
  if (it != helpers_.end()) {
    return it->second;
  }
  return nullptr;  // Return nullptr if the helper is not found
}

std::shared_ptr<edm::ProviderTransitionRecordIndexHelper> edm::TransitionRecordIndexHelpersBuilder::providerHelperFor(
    TransitionRecordKey const& key) const {
  auto it = providerHelpers_.find(key);
  if (it != providerHelpers_.end()) {
    return it->second;
  }
  return nullptr;  // Return nullptr if the helper is not found
}
