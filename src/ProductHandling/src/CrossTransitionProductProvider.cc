#include "ProductHandling/CrossTransitionProductProvider.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  CrossTransitionProductProvider::CrossTransitionProductProvider(TransitionRecordKey const& iReactTrasition,
                                                                 TransitionRecordKey const& iOtherTransition,
                                                                 std::vector<ProductProviderBase*> iProviders,
                                                                 std::vector<ProductKey> const& productsToProvide)
      : reactTransitionKey_(iReactTrasition),
        otherTransitionKey_(iOtherTransition),
        providers_(std::move(iProviders)),
        productsToProvide_(productsToProvide) {
    addProviderForProducts(providers_.front());
  }

  TransitionRecordKey CrossTransitionProductProvider::recordForProductsProvided() const { return otherTransitionKey_; }
  std::vector<ProductKey> CrossTransitionProductProvider::productsProvided() const { return productsToProvide_; }

  void CrossTransitionProductProvider::provideProductRequestAsync(WaitingTaskHolder task, TransitionContext& context) {
    context_ = &context;
    auto instance = context.get(otherTransitionKey_)->replicationIndex();
    pendingTask_ = std::move(task);
    providers_[instance]->provideProductRequestAsync(*pendingTask_, context, this);
  }
  void CrossTransitionProductProvider::reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context) {
    notifyConsumersProductsAvailableAsync(std::move(*pendingTask_), *context_);
    pendingTask_.reset();
    context_ = nullptr;
  }

  TransitionRecordKey CrossTransitionProductProvider::reactsToRecord() const { return reactTransitionKey_; }

  std::vector<TransitionRecordKey> CrossTransitionProductProvider::recordForProductsConsumed() const {
    return {otherTransitionKey_};
  }
  std::vector<ProductKey> CrossTransitionProductProvider::productsConsumed(TransitionRecordKey const&) const {
    return productsToProvide_;
  }

  void CrossTransitionProductProvider::resetProvider_() {
   resetConsumerForNewTransition();
  }
}  // namespace edm
