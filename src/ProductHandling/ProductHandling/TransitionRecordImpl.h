#ifndef ProductHandling_TransitionRecordImpl_h
#define ProductHandling_TransitionRecordImpl_h

#include "DataModel/ProductKey.h"
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductTransitionRecordIndex.h"
#include "DataProductBase/WrapperBase.h"

#include <memory>
#include <vector>
#include <cassert>
/*
TransitionRecordImpl does not hold a TransitionContext as this record can be shared across multiple contexts
IFF the TransitionRecords do not strictly form a hierarchy.

Q: Should the transition record hold connections to the TransitionRecordImpl for which it is a dependency?
*/
namespace edm {
  class ProductTransitionRecordIndexHelper;
  class TransitionRecordImpl {
  public:
    TransitionRecordImpl(TransitionRecordKey key,
                         std::shared_ptr<ProductTransitionRecordIndexHelper> iHelper,
                         unsigned int replicationIndex);

    WrapperBase const* get(ProductTransitionRecordIndex index) const {
      if (index.isUninitialized()) {
        return nullptr;
      }
      assert(index.value() < wrappers_.size());
      return wrappers_[index.value()].get();
    }

    ProductTransitionRecordIndexHelper const& helper() const { return *helper_; }

    TransitionRecordKey const& key() const { return key_; }

    //this is StreamID in Event, iovIndex in EventSetup and index for Run/LuminosityBlock
    unsigned int replicationIndex() const { return replicationIndex_; }

    /**If you are caching data from the Record, you should also keep
          this number.  If this number changes then you know that
          the data you have cached is invalid. This is NOT true if
          if the validityInterval() hasn't changed since it is possible that
          the job has gone to a new Record and then come back to the
          previous SyncValue and your algorithm didn't see the intervening
          Record.
          The value of '0' will never be returned so you can use that to
          denote that you have not yet checked the value.
          */
    unsigned long long cacheIdentifier() const { return cacheIdentifier_; }

  private:
    TransitionRecordKey key_;
    std::vector<std::unique_ptr<WrapperBase>> wrappers_;
    std::shared_ptr<ProductTransitionRecordIndexHelper> helper_;
    unsigned long long cacheIdentifier_ = 0;  // Initialized to 0 to indicate uninitialized state
    unsigned int replicationIndex_ = 0;       // Initialized to 0, can be set later if needed
  };
}  // namespace edm

#endif