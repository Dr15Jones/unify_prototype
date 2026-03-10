#ifndef ComponentBase_TransitionStateForActionSingleBase_h
#define ComponentBase_TransitionStateForActionSingleBase_h
#include "ComponentBase/TransitionStateForActionBase.h"
#include "ProductHandling/ProductConsumerSingleBase.h"

namespace edm {
  class TransitionStateForActionSingleBase : public ProductConsumerSingleBase<TransitionStateForActionBase> {
  public:
    TransitionStateForActionSingleBase() = default;
    ~TransitionStateForActionSingleBase() override = default;

  protected:
    /// @brief Must be called by derived classes when task started by workAsync is done
    /// @param result the result of the action
    void doneWorkAsync(WaitingTaskHolder task, TransitionProcessingContext const& context, ActionResult result);

  private:
    // ProductConsumerBase interface
    void reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const& context) final;
    // ProductProviderBase interface
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const& context) final;

    // DecisionNodeBase interface
    void makeDecisionAsync_(WaitingTaskHolder task,
                            TransitionProcessingContext const& context,
                            RequestState state) final;
    void willNotRunAsync(WaitingTaskHolder task, TransitionProcessingContext const& context);
    /// @brief Checks conditions to decide if workAsync should be called
    /// @return true if workAsync was called
    bool checkIfShouldRunAsync(WaitingTaskHolder task, TransitionProcessingContext const& context);

    /// @brief The actual work to be done by the action. Once the work is done
    /// the derived class must call doneWorkAsync to notify completion
    virtual void workAsync(WaitingTaskHolder task, TransitionProcessingContext const& context) = 0;

    void resetAction_() final;
    virtual void reset_() {};

    std::atomic<unsigned int> waitingConditionsToRun_{2};
  };
}  // namespace edm
#endif