#ifndef ProductHandling_ProductsProvider_h
#define ProductHandling_ProductsProvider_h

#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"

#include <vector>
namespace edm {
  class ProductResolverBase;
  class ProductsProvider {
  public:
    virtual ~ProductsProvider() = default;

    virtual std::vector<TransitionRecordKey> resolverRecords() const = 0;
    virtual std::vector<ProductKey> productKeysForRecord(TransitionRecordKey const&) const = 0;

  };
}  // namespace edm

#endif