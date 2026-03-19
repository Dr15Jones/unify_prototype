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
    virtual unsigned int numberOfProvidersForRecord(TransitionRecordKey const& record) const = 0;
    virtual std::vector<ProductKey> productsFromProvider(TransitionRecordKey const& record, unsigned int providerIndex) const = 0;
  };
}  // namespace edm

#endif