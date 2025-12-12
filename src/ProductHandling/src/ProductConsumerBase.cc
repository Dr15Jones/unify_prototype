#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/ProductProviderBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "DataModel/TransitionRecordKey.h"

namespace edm {

  std::optional<TransitionRecordKey> ProductConsumerBase::relatedStream() const { return std::nullopt; }

  void ProductConsumerBase::addProviderForProducts(ProductProviderBase* iProvider) {
    providers_.push_back(iProvider);
    iProvider->addConsumer(this);
  }

  void ProductConsumerBase::requestActionAsync(WaitingTaskHolder task, TransitionContext& context) {
    bool expected = false;
    if (haveRequestedProducts_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      if (not areAllProductsAvaliable()) {
        for (auto* provider : providers_) {
          provider->provideProductRequestAsync(task, context);
        }
      } else {
        //no providers, so we are done
        reactToAllProductsAvailableAsync(std::move(task), context);
      }
    }
  }

  void ProductConsumerBase::notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context) {
    size_t doneCount = providersDoneCount.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (doneCount == providers_.size() and haveRequestedProducts_.load(std::memory_order_acquire)) {
      reactToAllProductsAvailableAsync(task, context);
    }
  }

  void ProductConsumerBase::resetConsumerForNewTransition() {
    providersDoneCount.store(0,  std::memory_order_release);
    haveRequestedProducts_.store(false, std::memory_order_release);
    resetConsumer_();
  }
}  // namespace edm