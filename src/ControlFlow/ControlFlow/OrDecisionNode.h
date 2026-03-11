#ifndef ControlFlow_OrDecisionNode_h
#define ControlFlow_OrDecisionNode_h
/*
Decision node that implements logical OR behavior.
The decision is SKIP if all requestors decide SKIP.
If any requestor decides GO, the decision is GO.
If any requestor decides EXCEPTION, the decision is EXCEPTION.
*/

#include "ControlFlow/BinaryDecisionNode.h"
#include "ControlFlow/ControlFlowStatus.h"

namespace edm {
  class OrDecisionNode : public BinaryDecisionNode {
  public:
    OrDecisionNode(std::shared_ptr<DecisionNodeBase> iLeft, std::shared_ptr<DecisionNodeBase> iRight):
      BinaryDecisionNode(iLeft, iRight) {}
    virtual ~OrDecisionNode() final = default;
    private:
      ControlFlowStatus fullDecisionLogic_(ControlFlowStatus left, ControlFlowStatus right) const final;
      ControlFlowStatus shortCircuitLogic_(ControlFlowStatus decision) const final;
  };
}  // namespace edm
#endif //ControlFlow_OrDecisionNode_h