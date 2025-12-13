#include "ProductHandling/CrossTransitionProductProvider.h"
#include "ProductHandling/CrossTransitionProductConsumer.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  CrossTransitionProductProvider::CrossTransitionProductProvider(TransitionRecordKey const& iOtherTransition,
                                                                 unsigned int nTransitionInstances,
                                                                 std::vector<ProductKey> const& productsToProvide)
      : otherTransitionKey_(iOtherTransition), productsToProvide_(productsToProvide) {
    crossConsumers_.reserve(nTransitionInstances);
    for (unsigned int i = 0; i < nTransitionInstances; ++i) {
      crossConsumers_.emplace_back(std::make_unique<CrossTransitionProductConsumer>(this));
    }
  }

  void CrossTransitionProductProvider::resetProvider() { triggered_.store(false, std::memory_order_release); }
  TransitionRecordKey CrossTransitionProductProvider::recordForProductsProvided() const { return otherTransitionKey_; }
  std::vector<ProductKey> CrossTransitionProductProvider::productsProvided() const { return productsToProvide_; }
  void CrossTransitionProductProvider::provideProductRequestAsync(WaitingTaskHolder task, TransitionContext& context) {
    bool expected = false;
    if (triggered_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      context_ = &context;
      auto instance = context.get(otherTransitionKey_)->replicationIndex();
      pendingTask_ = std::move(task);
      crossConsumers_[instance]->trigger_requestDataAsync(*pendingTask_, context);
    }
  }
  void CrossTransitionProductProvider::ready() {
    notifyConsumersProductsAvailableAsync(std::move(*pendingTask_), *context_);
    pendingTask_.reset();
    context_ = nullptr;
  }

}  // namespace edm
