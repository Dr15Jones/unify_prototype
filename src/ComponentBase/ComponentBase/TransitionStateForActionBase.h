#ifndef ComponentBase_TransitionStateForActionBase_h
#define ComponentBase_TransitionStateForActionBase_h

#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/ProductProviderBase.h"
#include "ControlFlow/DecisionNodeBase.h"
#include "ComponentBase/ActionResult.h"

namespace edm {
  class TransitionStateForActionBase : public DecisionNodeBase, public ProductConsumerBase, public ProductProviderBase {
  public:
    TransitionStateForActionBase() = default;
    ~TransitionStateForActionBase() override = default;

    // ProductConsumerBase interface
    //TransitionRecordKey reactsToRecord() const override = 0;
    //std::vector<TransitionRecordKey> recordForProductsConsumed() const override = 0;
    //std::vector<ProductKey> productsConsumed(TransitionRecordKey const& record) const override = 0;

    // ProductProviderBase interface
    //TransitionRecordKey recordForProductsProvided() const override = 0;
    //std::vector<ProductKey> productsProvided() const override = 0;

    void resetForNewTransition() {
      DecisionNodeBase::resetForNewTransition();
      resetConsumerForNewTransition();
      // ProductProviderBase has no state to reset
      resetAction_();
    }

  private:
    virtual void resetAction_() = 0;
  };
}  // namespace edm

#endif