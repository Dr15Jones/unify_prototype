#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/ProductProviderBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "DataModel/TransitionRecordKey.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "ProductHandling/TransitionProductProviders.h"
namespace edm {

  std::optional<TransitionRecordKey> ProductConsumerBase::relatedStream() const { return std::nullopt; }

  void ProductConsumerBase::addProviderForProducts(TransitionRecordKey iTrans, TransitionProductProviderIndex iIndex) {
    providers_.push_back(std::make_pair(iTrans, iIndex));
  }

  void ProductConsumerBase::requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
    bool expected = false;
    if (haveRequestedProducts_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      if (not areAllProductsAvaliable()) {
        for (auto const& providerInfo : providers_) {
          auto const* providers = context.providerContext().get(providerInfo.first);
          if(providers) {
            auto provider = providers->providerForIndex(providerInfo.second);
            if (provider) {
              provider->provideProductRequestAsync(task, context, this);
            }
          }
        }
      } else {
        //no providers, so we are done
        reactToAllProductsAvailableAsync(std::move(task), context);
      }
    }
  }

  void ProductConsumerBase::notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
    size_t doneCount = providersDoneCount.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (doneCount == providers_.size()) {
      reactToAllProductsAvailableAsync(task, context);
    }
  }

  void ProductConsumerBase::resetConsumerForNewTransition() {
    providersDoneCount.store(0,  std::memory_order_release);
    haveRequestedProducts_.store(false, std::memory_order_release);
    resetConsumer_();
  }
}  // namespace edm