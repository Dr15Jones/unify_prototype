#ifndef ProductHandling_ProductConsumerSingleBase_h
#define ProductHandling_ProductConsumerSingleBase_h

#include <vector>
#include <optional>

#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"
#include "ProductHandling/TransitionProductProviderIndex.h"
#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  class WaitingTaskHolder;
  class TransitionContext;
  class TransitionProcessingContext;
  class TransitionRecordKey;
  class TransitionProductProviderIndex;
  template <typename T = ProductConsumerBase>
    requires std::derived_from<T, ProductConsumerBase>
  class ProductConsumerSingleBase : public T {
  public:
    ProductConsumerSingleBase() = default;
    virtual ~ProductConsumerSingleBase() = default;

    /// @brief Called by the scheduler to inform the consumer about which provider to use to get the products
    void addProviderForProducts(TransitionRecordKey iTrans,
                                ProductKey,
                                TransitionProductProviderIndex iIndex) override {
      for (auto const& providerInfo : providers_) {
        if (providerInfo.first == iTrans && providerInfo.second == iIndex) {
          return;
        }
      }
      providers_.push_back(std::make_pair(iTrans, iIndex));
    }

    /// @brief Called by a provider to notify that data products are available
    void notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext& context) override {
      size_t doneCount = providersDoneCount.fetch_add(1, std::memory_order_acq_rel) + 1;
      if (doneCount == providers_.size()) {
        reactToAllProductsAvailableAsync(task, context);
      }
    }

    /// @brief Called by the scheduler when the consumer's action is to be performed maybe called several times
    /// will trigger requests to all providers if products are not yet available
    /// @param task
    /// @param context
    void requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext& context) override {
      bool expected = false;
      if (haveRequestedProducts_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        if (not areAllProductsAvaliable()) {
          for (auto const& providerInfo : providers_) {
            auto const* providers = context.providerContext().get(providerInfo.first);
            if (providers) {
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

    void resetConsumerForNewTransition() final {
      providersDoneCount.store(0, std::memory_order_release);
      haveRequestedProducts_.store(false, std::memory_order_release);
      resetConsumer_();
    }

  protected:
    bool areAllProductsAvaliable() const { return providersDoneCount.load() == providers_.size(); }

  private:
    virtual void resetConsumer_() {};
    /// @brief Called when data is available from all providers and requestActionAsync has been called
    /// @param task
    /// @param context
    virtual void reactToAllProductsAvailableAsync(WaitingTaskHolder task,
                                                  TransitionProcessingContext& context) = 0;
    std::vector<std::pair<TransitionRecordKey, TransitionProductProviderIndex>> providers_;
    std::atomic<size_t> providersDoneCount{0};
    std::atomic<bool> haveRequestedProducts_{false};
  };

}  // namespace edm

#endif