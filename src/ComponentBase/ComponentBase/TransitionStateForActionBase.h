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
      ProductConsumerBase::resetConsumerForNewTransition();
      // ProductProviderBase has no state to reset
      waitingConditionsToRun_.store(2, std::memory_order_release);
      reset_();
    }

  protected:
    /// @brief Must be called by derived classes when task started by workAsync is done
    /// @param result the result of the action
    void doneWorkAsync(WaitingTaskHolder task, TransitionProcessingContext const & context, ActionResult result);

    virtual void reset_() {};

  private:
    // ProductConsumerBase interface
    void reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) final;
    // ProductProviderBase interface
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) final;

  // DecisionNodeBase interface
    void makeDecisionAsync_(WaitingTaskHolder task, TransitionProcessingContext const & context, RequestState state) final;
    void willNotRunAsync(WaitingTaskHolder task, TransitionProcessingContext const & context);
    /// @brief Checks conditions to decide if workAsync should be called
    /// @return true if workAsync was called 
    bool checkIfShouldRunAsync(WaitingTaskHolder task, TransitionProcessingContext const & context);

    /// @brief The actual work to be done by the action. Once the work is done
    /// the derived class must call doneWorkAsync to notify completion
    virtual void workAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) = 0;

    std::atomic<unsigned int> waitingConditionsToRun_{2};
  };
}  // namespace edm

#endif