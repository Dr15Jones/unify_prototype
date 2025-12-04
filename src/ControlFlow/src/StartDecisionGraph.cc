#include "ControlFlow/DecisionRequestState.h"
#include "ControlFlow/DecisionNodeBase.h"
#include "ControlFlow/StartDecisionGraph.h"
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  void StartDecisionGraph::startAsync(WaitingTaskHolder task, TransitionContext& context) {
    for (auto leaf : leafNodes_) {
      leaf->requestDecisionAsync(task, context, RequestState::REQUEST_DECISION);
    }
  }

  void StartDecisionGraph::addLeafNode(DecisionNodeBase* iLeaf) {
    leafNodes_.push_back(iLeaf);
    iLeaf->addRequestorForDecision(this);
  }


  void StartDecisionGraph::decisionFromNodeAsync(WaitingTaskHolder task,
                                                 TransitionContext& context,
                                                 void const* nodeHash,
                                                 ControlFlowStatus decision) {
    auto finishedLeafNodes = finishedLeafNodes_.fetch_add(1, std::memory_order_acq_rel);
  }
}  // namespace edm