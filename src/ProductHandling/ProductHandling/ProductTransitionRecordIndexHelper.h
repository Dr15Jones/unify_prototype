#ifndef ProductHandling_ProductTransitionRecordIndexHelper_h
#define ProductHandling_ProductTransitionRecordIndexHelper_h

#include "DataModel/ProductTransitionRecordIndex.h"
#include "DataModel/ProductKey.h"

#include <map>
#include <vector>
namespace edm {
  class ProductTransitionRecordIndexHelper {
  public:
    ProductTransitionRecordIndexHelper() = default;

    void insert(ProductKey const& productKey);
    void finalize(std::vector<std::string_view> const& processNameOrder);
    ProductTransitionRecordIndex getIndex(ProductKey const& productKey) const;
    size_t numberOfProducts() const { return keyToIndex_.size(); }

  private:
    std::map<ProductKey, ProductTransitionRecordIndex> keyToIndex_;
  };
}  // namespace edm

#endif