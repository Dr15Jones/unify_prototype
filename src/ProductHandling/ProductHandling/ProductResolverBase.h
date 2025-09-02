#ifndef ProductHandling_ProductResolverBase_h
#define ProductHandling_ProductResolverBase_h

#include "Concurrency/WaitingTaskHolder.h"

#include <memory>

namespace edm {
  class TransitionRecordImpl;
  class TransitionContext;
  class WrapperBase;
  class ProductResolverBase {
  public:
    virtual ~ProductResolverBase() = default;
    virtual void prefetchAsync(WaitingTaskHolder waitTask,
                               TransitionRecordImpl const &record,
                               TransitionContext const &context) const noexcept = 0;

    virtual std::shared_ptr<WrapperBase> getAfterPrefetch() const = 0;

    virtual void invalidate() noexcept = 0;
  };
}  // namespace edm

#endif