#include "ProductHandling/ProductProviderBase.h"
#include "ProductHandling/ProductConsumerBase.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  void ProductProviderBase::notifyConsumersProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context) {
    for (auto* consumer : consumers_) {
      consumer->notifyProductsAvailableAsync(task, context);
    }
  }

}  // namespace edm