#include "ControlFlow/DecisionNodeBase.h"
#include "ControlFlow/DecisionRequestorBase.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  void DecisionNodeBase::requestDecisionAsync(WaitingTaskHolder iTask,
                                              TransitionContext& iContext,
                                              RequestState iState) {
    if (iState == RequestState::REQUEST_DECISION) {
      //This need to be thread safe against multiple requestors calling requestingDecisionAsync
      if (runStatus_ == RunStatus::NOT_STARTED) {
        runStatus_ = RunStatus::SHOULD_RUN;
        makeDecisionAsync_(std::move(iTask), iContext, iState);
      }
    } else {
      if (runStatus_ == RunStatus::NOT_STARTED) {
        auto willNotRunRequests = ++willNotRunRequests_;
        if (willNotRunRequests == requestors_.size()) {
          //if all requestors tell us they will not run, we will not run
          runStatus_ = RunStatus::WILL_NOT_RUN;
          makeDecisionAsync_(std::move(iTask), iContext, iState);
        }
      }
    }
  }
  void DecisionNodeBase::sendDecisionToRequestorsAsync_(WaitingTaskHolder iTask,
                                                        TransitionContext& iContext,
                                                        ControlFlowStatus iDecision) {
    //This should be called once the task started from decisionRequestedAsync_ has the final decision
    if (runStatus_ != RunStatus::WILL_NOT_RUN) {
      runStatus_ = RunStatus::COMPLETED;
    }
    for (auto requestor : requestors_) {
      requestor->decisionFromNodeAsync(iTask, iContext, this, iDecision);
    }
  }

}  // namespace edm