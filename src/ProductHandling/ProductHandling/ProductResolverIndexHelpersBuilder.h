#ifndef ProductHandling_ProductResolverIndexHelpersBuilder_h
#define ProductHandling_ProductResolverIndexHelpersBuilder_h

#include "ProductHandling/ProductResolverIndexHelper.h"
#include "DataModel/TransitionRecordKey.h"
#include <map>
#include <vector>
#include <memory>
#include <string_view>
namespace edm {
  class ProductResolversProvider;

  class ProductResolverIndexHelpersBuilder {
  public:
    ProductResolverIndexHelpersBuilder() = default;

    void determineProductsFrom(ProductResolversProvider const& provider);

    void finalize(std::vector<std::string_view> const& processNameOrdering);
    std::vector<TransitionRecordKey> usedRecords() const;

    std::shared_ptr<ProductResolverIndexHelper> helperFor(TransitionRecordKey const& key) const;

  private:
    std::map<TransitionRecordKey, std::shared_ptr<ProductResolverIndexHelper>> helpers_;
  };
}  // namespace edm

#endif