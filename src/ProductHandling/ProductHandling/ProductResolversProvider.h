#ifndef ProductHandling_ProductResolversProvider_h
#define ProductHandling_ProductResolversProvider_h

#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"

#include <vector>
namespace edm {
  class ProductResolverBase;
  class ProductResolversProvider {
  public:
    virtual ~ProductResolversProvider() = default;

    virtual std::vector<TransitionRecordKey> resolverRecords() const = 0;
    virtual std::vector<ProductKey> productKeysForRecord(TransitionRecordKey const&) const = 0;

    virtual std::unique_ptr<ProductResolverBase> makeResolver(
        TransitionRecordKey const& recordKey,
        ProductKey const& productKey) const = 0;
  };
}  // namespace edm

#endif