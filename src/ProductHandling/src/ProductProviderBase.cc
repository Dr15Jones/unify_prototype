#include "ProductHandling/ProductProviderBase.h"
#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  void ProductProviderBase::provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const & context, ProductConsumerBase* consumer) {
    bool expectedRequested_ = false;
    if( requested_.compare_exchange_strong(expectedRequested_, true) ) {
      provideProductRequestAsync(task, context);
    }
    if( productsAvailable_ ) {
        consumer->notifyProductsAvailableAsync(task, context);
    } else {
      waitingConsumers_.push(consumer);
      if( productsAvailable_ ) {
        while( waitingConsumers_.try_pop(consumer) ) {
          consumer->notifyProductsAvailableAsync(task, context);
        }
      }
    }
  }

  void ProductProviderBase::notifyConsumersProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
    ProductConsumerBase* consumer;
    productsAvailable_ = true;
    while( waitingConsumers_.try_pop(consumer) ) {
        consumer->notifyProductsAvailableAsync(task, context);
    }
  }

  void ProductProviderBase::resetProvider() {
    requested_ = false;
    productsAvailable_ = false;
    resetProvider_();
  }
}  // namespace edm