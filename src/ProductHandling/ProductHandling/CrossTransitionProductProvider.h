#ifndef ProductHandling_CrossTransitionProductProvider_h
#define ProductHandling_CrossTransitionProductProvider_h
/*
A ProductProvider that is used to obtain products from a different Tranition.
This works in conjunction with a CrossTransitionProductConsumer.
This Provider lives in the Transition of the Consumer and requests products via
  the CrossTransitionProductConsumer from the other Transition.
*/

#include <vector>
#include <memory>
#include <optional>
#include "ProductHandling/ProductProviderBase.h"
#include "DataModel/TransitionRecordKey.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  class CrossTransitionProductConsumer;

  class CrossTransitionProductProvider : public ProductProviderBase {
  public:
    CrossTransitionProductProvider(TransitionRecordKey const& iOtherTransition,
                                   unsigned int nTransitionInstances,
                                   std::vector<ProductKey> const& productsToProvide);
    ~CrossTransitionProductProvider() override = default;
    TransitionRecordKey recordForProductsProvided() const final;
    std::vector<ProductKey> productsProvided() const final;
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionContext& context) final;

    void resetProvider() final;
    //Called by the CrossTransitionProductConsumer to notify that data is available
    void ready();

    std::vector<std::unique_ptr<CrossTransitionProductConsumer>> const& crossConsumers() { return crossConsumers_; }

  private:
    TransitionRecordKey otherTransitionKey_;
    std::vector<ProductKey> productsToProvide_;
    std::vector<std::unique_ptr<CrossTransitionProductConsumer>> crossConsumers_;
    TransitionContext* context_{nullptr};
    std::optional<WaitingTaskHolder> pendingTask_;
    std::atomic<bool> triggered_{false};
  };
}  // namespace edm
#endif