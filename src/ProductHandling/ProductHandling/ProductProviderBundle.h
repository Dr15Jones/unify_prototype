#ifndef ProductHandling_ProductProviderBundle_h
#define ProductHandling_ProductProviderBundle_h

#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"

#include <vector>
namespace edm {
  class ProductResolverBase;
  class ProductProviderBundle {
  public:
    virtual ~ProductProviderBundle() = default;

    virtual std::vector<TransitionRecordKey> resolverRecords() const = 0;
    virtual unsigned int numberOfProvidersForRecord(TransitionRecordKey const& record) const = 0;
    virtual std::vector<ProductKey> productsFromProvider(TransitionRecordKey const& record, unsigned int providerIndex) const = 0;
  };
}  // namespace edm

#endif