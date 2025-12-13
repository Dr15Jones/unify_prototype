#ifndef ProductHandling_CrossTransitionProductConsumer_h
#define ProductHandling_CrossTransitionProductConsumer_h
/*
A ProductConsumer that is used to obtain products from a different Tranition.
This works in conjunction with a CrossTransitionProductProvider.
This Consumer lives in the Transition of the Provider and provides products via
  the CrossTransitionProductProvider to the other Transition.
*/
#include <atomic>
#include "ProductHandling/ProductConsumerBase.h"

namespace edm {
  class CrossTransitionProductProvider;

  class CrossTransitionProductConsumer : public ProductConsumerBase {
  public:
    CrossTransitionProductConsumer(CrossTransitionProductProvider* iProvider) : provider_(iProvider) {}
    ~CrossTransitionProductConsumer() override = default;
    TransitionRecordKey reactsToRecord() const final;
    std::vector<TransitionRecordKey> recordForProductsConsumed() const final;
    std::vector<ProductKey> productsConsumed(TransitionRecordKey const&) const final;

    //Called by the CrossTransitionProductProvider to trigger the request for data
    void trigger_requestDataAsync(WaitingTaskHolder task, TransitionContext& context);

  protected:
    void reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionContext& context) final;

  private:
    void callReadyIfDataAvailable();
    void resetConsumer_() final;
    CrossTransitionProductProvider* provider_{nullptr};
    std::atomic<bool> haveRequestedProducts_{false};
    std::atomic<bool> productsAvailableNotified_{false};
  };
}  // namespace edm
#endif