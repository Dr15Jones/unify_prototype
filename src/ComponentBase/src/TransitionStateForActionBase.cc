#include "ComponentBase/TransitionStateForActionBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ControlFlow/DecisionRequestState.h"
#include "ProductHandling/TransitionProcessingContext.h"

namespace edm {
  void TransitionStateForActionBase::makeDecisionAsync_(WaitingTaskHolder task,
                                                        TransitionProcessingContext const & context,
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

  bool TransitionStateForActionBase::checkIfShouldRunAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
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

  void TransitionStateForActionBase::provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
    if (not checkIfShouldRunAsync(task, context)) {
      requestActionAsync(std::move(task), context);
    }
  }
  void TransitionStateForActionBase::reactToAllProductsAvailableAsync(WaitingTaskHolder task,
                                                                      TransitionProcessingContext const & context) {
    (void)checkIfShouldRunAsync(task, context);
  }

  void TransitionStateForActionBase::willNotRunAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) {
    notifyConsumersProductsAvailableAsync(task, context);
  }

  void TransitionStateForActionBase::doneWorkAsync(WaitingTaskHolder task,
                                                    TransitionProcessingContext const & context,
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
}  // namespace edm