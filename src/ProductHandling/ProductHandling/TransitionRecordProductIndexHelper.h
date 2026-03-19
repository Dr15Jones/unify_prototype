#ifndef ProductHandling_TransitionRecordProductIndexHelper_h
#define ProductHandling_TransitionRecordProductIndexHelper_h

#include "DataModel/TransitionRecordProductIndex.h"
#include "DataModel/ProductKey.h"

#include <map>
#include <vector>
namespace edm {
  class TransitionRecordProductIndexHelper {
  public:
    TransitionRecordProductIndexHelper() = default;

    void insert(ProductKey const& productKey);
    void finalize(std::vector<std::string_view> const& processNameOrder);
    TransitionRecordProductIndex getIndex(ProductKey const& productKey) const;
    size_t numberOfProducts() const { return keyToIndex_.size(); }

  private:
    std::map<ProductKey, TransitionRecordProductIndex> keyToIndex_;
  };
}  // namespace edm

#endif