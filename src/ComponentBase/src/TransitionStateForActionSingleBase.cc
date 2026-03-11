#include "ComponentBase/TransitionStateForActionSingleBase.h"

namespace edm {
  void TransitionStateForActionSingleBase::makeDecisionAsync_(WaitingTaskHolder task,
                                                              TransitionProcessingContext& context,
                                                              RequestState state) {
    if (state == RequestState::REQUEST_DECISION) {
      bool shouldRun = checkIfShouldRunAsync(task, context);
      if (not shouldRun) {
        requestActionAsync(std::move(task), context);
      }
    } else {
      willNotRunAsync(std::move(task), context);
    }
  }

  bool TransitionStateForActionSingleBase::checkIfShouldRunAsync(WaitingTaskHolder task,
                                                                 TransitionProcessingContext& context) {
    unsigned int prev = waitingConditionsToRun_.fetch_sub(1, std::memory_order_acq_rel);
    if (not makeDecisionWillBeCalled()) {
      // No decision to be made, so we can consider that condition satisfied
      prev -= 1;
    }
    if (prev == 1) {
      // Both conditions are satisfied
      workAsync(std::move(task), context);
      return true;
    }
    return false;
  }

  void TransitionStateForActionSingleBase::provideProductRequestAsync(WaitingTaskHolder task,
                                                                      TransitionProcessingContext& context) {
    if (not checkIfShouldRunAsync(task, context)) {
      requestActionAsync(std::move(task), context);
    }
  }

  void TransitionStateForActionSingleBase::willNotRunAsync(WaitingTaskHolder task,
                                                           TransitionProcessingContext& context) {
    notifyConsumersProductsAvailableAsync(task, context);
  }

  void TransitionStateForActionSingleBase::doneWorkAsync(WaitingTaskHolder task,
                                                         TransitionProcessingContext& context,
                                                         ActionResult result) {
    setCompletedForDiagnostics();
    edm::ControlFlowStatus status = edm::ControlFlowStatus::NOT_STARTED;
    if (result.isAccept()) {
      status = edm::ControlFlowStatus::GO;
    } else if (result.isReject()) {
      status = edm::ControlFlowStatus::SKIP;
    } else if (result.isException()) {
      status = edm::ControlFlowStatus::EXCEPTION;
    }
    sendDecisionToRequestorsAsync_(task, context, status);
    notifyConsumersProductsAvailableAsync(std::move(task), context);
  }

  void TransitionStateForActionSingleBase::reactToAllProductsAvailableAsync(
      WaitingTaskHolder task, TransitionProcessingContext& context) {
    (void)checkIfShouldRunAsync(task, context);
  }

  void TransitionStateForActionSingleBase::resetAction_() {
    reset_();
    waitingConditionsToRun_.store(2, std::memory_order_release);
  }
}  // namespace edm