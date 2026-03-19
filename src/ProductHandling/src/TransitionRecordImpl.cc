#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/TransitionRecordProductIndexHelper.h"

namespace edm {
  TransitionRecordImpl::TransitionRecordImpl(TransitionRecordKey key,
                                             std::shared_ptr<TransitionRecordProductIndexHelper> iHelper,
                                             unsigned int replicationIndex)
      : key_(key), wrappers_(iHelper->numberOfProducts()), helper_(iHelper), replicationIndex_(replicationIndex) {
    assert(helper_);
  }

  void TransitionRecordImpl::reset() {
    cacheIdentifier_ += 1;
    for (auto& wrapper : wrappers_) {
      wrapper.reset();
    }
  }
}  // namespace edm