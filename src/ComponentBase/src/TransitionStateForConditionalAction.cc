#include "ComponentBase/TransitionStateForConditionalAction.h"

namespace edm {
  void TransitionStateForConditionalAction::notifyProductsAvailableAsync(WaitingTaskHolder task,
                                                                         TransitionProcessingContext& context) {
    assert(false);
  }

  void TransitionStateForConditionalAction::provideProductRequestAsync(WaitingTaskHolder task,
                                                                       TransitionProcessingContext& context) {
    assert(false);
  }

  void TransitionStateForConditionalAction::makeDecisionAsync_(WaitingTaskHolder task,
                                                               TransitionProcessingContext& context,
                                                               RequestState state) {
    decisionNode_.requestDecisionAsync(std::move(task), context, state);
  }

  void TransitionStateForConditionalAction::requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext & context) {
    assert(false);
  }

  void TransitionStateForConditionalAction::resetConsumerForNewTransition() {
    decision_->resetForNewTransition();
    action_->resetForNewTransition();
  }

  void TransitionStateForConditionalAction::resetAction_() {
    decision_->resetForNewTransition();
    action_->resetForNewTransition();
    decisionNode_.resetForNewTransition();
  }

}  // namespace edm