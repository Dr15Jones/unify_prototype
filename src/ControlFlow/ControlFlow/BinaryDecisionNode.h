#ifndef ControlFlow_BinaryDecisionNode_h
#define ControlFlow_BinaryDecisionNode_h
/*
Decision node that implements logical AND behavior.
The decision is GO if all requestors decide GO.
If any requestor decides SKIP, the decision is SKIP.
If any requestor decides EXCEPTION, the decision is EXCEPTION.
*/

#include "ControlFlow/DecisionRequestorBase.h"
#include "ControlFlow/DecisionNodeBase.h"
#include "ControlFlow/ControlFlowStatus.h"

#include <memory>
namespace edm {
  class BinaryDecisionNode : public DecisionNodeBase, public DecisionRequestorBase {
  public:
    BinaryDecisionNode(std::shared_ptr<DecisionNodeBase>& iLeft, std::shared_ptr<DecisionNodeBase>& iRight);
    ~BinaryDecisionNode() override = default;
    void decisionFromNodeAsync(WaitingTaskHolder task,
                                        TransitionContext& context,
                                        void const* nodeHash,
                                        ControlFlowStatus decision) override;

  private:
    void makeDecisionAsync_(WaitingTaskHolder task, TransitionContext& context, RequestState state) override;

    virtual ControlFlowStatus fullDecisionLogic_(ControlFlowStatus left, ControlFlowStatus right) const = 0;
    virtual ControlFlowStatus shortCircuitLogic_(ControlFlowStatus decision) const = 0;
    std::shared_ptr<DecisionNodeBase> leftNode_;
    std::shared_ptr<DecisionNodeBase> rightNode_;
    int decisionsFromComposites_ = 0;
    int requestCallsToComposites_ = 0;
    ControlFlowStatus leftDecision_ = ControlFlowStatus::NOT_STARTED;
    ControlFlowStatus rightDecision_ = ControlFlowStatus::NOT_STARTED;
  };
}  // namespace edm
#endif //ControlFlow_BinaryDecisionNode_h