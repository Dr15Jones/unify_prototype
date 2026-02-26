#ifndef ProductHandling_ProductProviderBase_h
#define ProductHandling_ProductProviderBase_h

#include <vector>
#include <atomic>
#include <oneapi/tbb/concurrent_queue.h>
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"

namespace edm {

  class ProductConsumerBase;
  class WaitingTaskHolder;
  class TransitionContext;
  class TransitionProcessingContext;

  class ProductProviderBase {
  public:
    ProductProviderBase() = default;
    virtual ~ProductProviderBase() = default;
    ProductProviderBase(ProductProviderBase&& other)
        : waitingConsumers_(std::move(other.waitingConsumers_)),
          requested_{other.requested_.load()},
          productsAvailable_{other.productsAvailable_.load()} {}
    ProductProviderBase& operator=(ProductProviderBase&& other) {
      if (this != &other) {
        waitingConsumers_ = std::move(other.waitingConsumers_);
        requested_.store(other.requested_.load());
        productsAvailable_.store(other.productsAvailable_.load());
      }
      return *this;
    }

    virtual TransitionRecordKey recordForProductsProvided() const = 0;
    virtual std::vector<ProductKey> productsProvided() const = 0;

    /// @brief Called by a consumer to request the data products
    /// @param task
    /// @param context
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const & context, ProductConsumerBase* consumer);

    void resetProvider();

  protected:
    /// @brief Call when products are available to notify consumers. This must be called once an only once per transition
    /// @param task
    /// @param context
    void notifyConsumersProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context);
    virtual void provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) = 0;

  private:
    virtual void resetProvider_() {}
    oneapi::tbb::concurrent_queue<ProductConsumerBase*> waitingConsumers_;
    std::atomic<bool> requested_{false};
    std::atomic<bool> productsAvailable_{false};
  };
}  // namespace edm
#endif