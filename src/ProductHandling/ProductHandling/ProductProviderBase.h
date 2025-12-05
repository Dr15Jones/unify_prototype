#ifndef ProductHandling_ProductProviderBase_h
#define ProductHandling_ProductProviderBase_h

#include <vector>
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/ProductKey.h"

namespace edm {

  class ProductConsumerBase;
  class WaitingTaskHolder;
  class TransitionContext;

  class ProductProviderBase {
  public:
    ProductProviderBase() = default;
    virtual ~ProductProviderBase() = default;

    virtual TransitionRecordKey recordForProducts() const = 0;
    virtual std::vector<ProductKey> productsProvided() const = 0;

    void addConsumer(ProductConsumerBase* iConsumer) { consumers_.push_back(iConsumer); }

    /// @brief Called by a consumer to request the data products
    /// @param task
    /// @param context
    virtual void provideProductRequestAsync(WaitingTaskHolder task, TransitionContext& context) = 0;

  protected:
    /// @brief Call when products are available to notify consumers
    /// @param task
    /// @param context
    void notifyConsumersProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context);

  private:
    std::vector<ProductConsumerBase*> consumers_;
  };
}  // namespace edm
#endif