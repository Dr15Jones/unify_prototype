#ifndef ProductHandling_TransitionProductProviderIndexHelper_h
#define ProductHandling_TransitionProductProviderIndexHelper_h

#include "ProductHandling/TransitionProductProviderIndex.h"
#include "DataModel/ProductKey.h"

#include <map>
#include <vector>
namespace edm {
  class TransitionProductProviderIndexHelper {
  public:
    TransitionProductProviderIndexHelper() = default;

    void insert(ProductKey const& productKey);
    TransitionProductProviderIndex getIndex(ProductKey const& productKey) const;
    size_t numberOfProviders() const { return keyToIndex_.size(); }

  private:
    std::map<ProductKey, TransitionProductProviderIndex> keyToIndex_;
  };
}  // namespace edm

#endif