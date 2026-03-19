#ifndef ProductHandling_TransitionRecordIndexHelpersBuilder_h
#define ProductHandling_TransitionRecordIndexHelpersBuilder_h

#include "ProductHandling/TransitionRecordProductIndexHelper.h"
#include "ProductHandling/TransitionRecordProductProviderIndexHelper.h"
#include "ProductHandling/ProvidersKey.h"
#include "DataModel/TransitionRecordKey.h"
#include <map>
#include <vector>
#include <memory>
#include <string_view>
namespace edm {
  class ProductsProvider;

  class TransitionRecordIndexHelpersBuilder {
  public:
    TransitionRecordIndexHelpersBuilder() = default;

    void determineProductsFrom(ProvidersKey const& key, ProductsProvider const& provider);

    void finalize(std::vector<std::string_view> const& processNameOrdering);
    std::vector<TransitionRecordKey> usedRecords() const;

    std::shared_ptr<TransitionRecordProductIndexHelper> helperFor(TransitionRecordKey const& key) const;
    std::shared_ptr<TransitionRecordProductProviderIndexHelper> providerHelperFor(TransitionRecordKey const& key) const;
  private:
    std::map<TransitionRecordKey, std::shared_ptr<TransitionRecordProductIndexHelper>> helpers_;
    std::map<TransitionRecordKey, std::shared_ptr<TransitionRecordProductProviderIndexHelper>> providerHelpers_;
  };
}  // namespace edm

#endif