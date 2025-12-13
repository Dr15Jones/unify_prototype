#include "ProductHandling/CrossTransitionProductConsumer.h"
#include "ProductHandling/CrossTransitionProductProvider.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  TransitionRecordKey CrossTransitionProductConsumer::reactsToRecord() const {
    return provider_->recordForProductsProvided();
  }
  std::vector<TransitionRecordKey> CrossTransitionProductConsumer::recordForProductsConsumed() const {
    return {provider_->recordForProductsProvided()};
  }
  std::vector<ProductKey> CrossTransitionProductConsumer::productsConsumed(TransitionRecordKey const&) const {
    return provider_->productsProvided();
  }

  void CrossTransitionProductConsumer::trigger_requestDataAsync(WaitingTaskHolder task, TransitionContext& context) {
    bool expected = false;
    if (haveRequestedProducts_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      //Request data from the other Transition via the Provider
      ProductConsumerBase::requestActionAsync(std::move(task), context);
    }
    callReadyIfDataAvailable();
  }

  void CrossTransitionProductConsumer::reactToAllProductsAvailableAsync(WaitingTaskHolder task,
                                                                        TransitionContext& context) {
    productsAvailableNotified_.store(true, std::memory_order_release);
    callReadyIfDataAvailable();
  }
  void CrossTransitionProductConsumer::callReadyIfDataAvailable() {
    if (haveRequestedProducts_.load(std::memory_order_acquire) and
        productsAvailableNotified_.load(std::memory_order_acquire)) {
      provider_->ready();
    }
  }
  void CrossTransitionProductConsumer::resetConsumer_() {
    haveRequestedProducts_.store(false, std::memory_order_release);
    productsAvailableNotified_.store(false, std::memory_order_release);
  }
}  // namespace edm