#ifndef ProductHandling_ProductTransitionRecordIndexHelpersBuilder_h
#define ProductHandling_ProductTransitionRecordIndexHelpersBuilder_h

#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
#include "DataModel/TransitionRecordKey.h"
#include <map>
#include <vector>
#include <memory>
#include <string_view>
namespace edm {
  class ProductsProvider;

  class ProductTransitionRecordIndexHelpersBuilder {
  public:
    ProductTransitionRecordIndexHelpersBuilder() = default;

    void determineProductsFrom(ProductsProvider const& provider);

    void finalize(std::vector<std::string_view> const& processNameOrdering);
    std::vector<TransitionRecordKey> usedRecords() const;

    std::shared_ptr<ProductTransitionRecordIndexHelper> helperFor(TransitionRecordKey const& key) const;

  private:
    std::map<TransitionRecordKey, std::shared_ptr<ProductTransitionRecordIndexHelper>> helpers_;
  };
}  // namespace edm

#endif