#ifndef ProductHandling_ProductResolverIndexHelper_h
#define ProductHandling_ProductResolverIndexHelper_h

#include "DataModel/ProductResolverIndex.h"
#include "DataModel/ProductKey.h"

#include <map>
#include <vector>
namespace edm {
  class ProductResolverIndexHelper {
  public:
    ProductResolverIndexHelper() = default;

    void insert(ProductKey const& productKey);
    void finalize(std::vector<std::string_view> const& processNameOrder);
    ProductResolverIndex getIndex(ProductKey const& productKey) const;

  private:
    std::map<ProductKey, ProductResolverIndex> keyToIndex_;
  };
}  // namespace edm

#endif