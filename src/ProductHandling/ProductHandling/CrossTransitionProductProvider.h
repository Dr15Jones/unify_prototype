#ifndef ProductHandling_CrossTransitionProductProvider_h
#define ProductHandling_CrossTransitionProductProvider_h
/*
A ProductProvider that is used to obtain products from a different Transition.
This Provider lives in the Transition of the Consumer and requests products via
  its ProductConsumerBase interface from the other Transition.
*/

#include <vector>
#include <memory>
#include <optional>
#include "ProductHandling/ProductProviderBase.h"
#include "ProductHandling/ProductConsumerBase.h"
#include "DataModel/TransitionRecordKey.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {

  class CrossTransitionProductProvider : public ProductProviderBase, public ProductConsumerBase {
  public:
    CrossTransitionProductProvider(TransitionRecordKey const& iReactTransition,
                                  TransitionRecordKey const& iOtherTransition,
                                   std::vector<ProductProviderBase*> iProviders,
                                   std::vector<ProductKey> const& productsToProvide);
    ~CrossTransitionProductProvider() override = default;
    TransitionRecordKey recordForProductsProvided() const final;
    std::vector<ProductKey> productsProvided() const final;
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionContext& context) final;

    //consumer interface
    TransitionRecordKey reactsToRecord() const final;
    std::vector<TransitionRecordKey> recordForProductsConsumed() const final;
    std::vector<ProductKey> productsConsumed(TransitionRecordKey const&) const final;

  private:
    void resetProvider_() final;
    //part of consumer interface
    void reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context) final;
    TransitionRecordKey reactTransitionKey_;
    TransitionRecordKey otherTransitionKey_;
    std::vector<ProductKey> productsToProvide_;
    std::vector<ProductProviderBase*> providers_;
    TransitionContext* context_{nullptr};
    std::optional<WaitingTaskHolder> pendingTask_;
  };
}  // namespace edm
#endif