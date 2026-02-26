#include "ControlFlow/BinaryDecisionNode.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  BinaryDecisionNode::BinaryDecisionNode(std::shared_ptr<DecisionNodeBase>& iLeft,
                                         std::shared_ptr<DecisionNodeBase>& iRight)
      : leftNode_{iLeft}, rightNode_{iRight} {
    leftNode_->addRequestorForDecision(this);
    rightNode_->addRequestorForDecision(this);
  }

  void BinaryDecisionNode::decisionFromNodeAsync(WaitingTaskHolder task,
                                                          TransitionProcessingContext const & context,
                                                          void const* nodeHash,
                                                          ControlFlowStatus decision) {
    if (nodeHash == leftNode_.get()) {
      leftDecision_ = decision;
    } else {
      rightDecision_ = decision;
    }
    //if this were a threaded program we need to set decision before the increment call
    decisionsFromComposites_ += 1;
    if (runStatus() == RunStatus::IN_PROGRESS) {
      if (decisionsFromComposites_ == 2) {
        sendDecisionToRequestorsAsync_(std::move(task), context, fullDecisionLogic_(leftDecision_, rightDecision_));
      } else if (nodeHash == leftNode_.get()) {
        ControlFlowStatus shortCircuit = shortCircuitLogic_(leftDecision_);
        if (shortCircuit != ControlFlowStatus::NOT_STARTED) {
          sendDecisionToRequestorsAsync_(task, context, shortCircuit);
          rightNode_->requestDecisionAsync(std::move(task), context, RequestState::NO_REQUEST_COMING);
        } else {
          rightNode_->requestDecisionAsync(std::move(task), context, RequestState::REQUEST_DECISION);
        }
      } else {
        //left has been called but isn't done yet BUT we might be able to get a jump on the decision
        ControlFlowStatus shortCircuit = shortCircuitLogic_(rightDecision_);
        if (shortCircuit != ControlFlowStatus::NOT_STARTED) {
          sendDecisionToRequestorsAsync_(std::move(task), context, shortCircuit);
        }
      }
    }
  }

  void BinaryDecisionNode::makeDecisionAsync_(WaitingTaskHolder task,
                                                   TransitionProcessingContext const & context,
                                                   RequestState state) {
    requestCallsToComposites_ = 1;
    setInProgressForDiagnostics();
    leftNode_->requestDecisionAsync(task, context, state);
    if (state == RequestState::NO_REQUEST_COMING) {
      rightNode_->requestDecisionAsync(std::move(task), context, state);
    }
  }
}  // namespace edm