#ifndef ControlFlow_AndDecisionNode_h
#define ControlFlow_AndDecisionNode_h
/*
Decision node that implements logical AND behavior.
The decision is GO if all requestors decide GO.
If any requestor decides SKIP, the decision is SKIP.
If any requestor decides EXCEPTION, the decision is EXCEPTION.
*/

#include "ControlFlow/BinaryDecisionNode.h"
#include "ControlFlow/ControlFlowStatus.h"

namespace edm {
  class AndDecisionNode final : public BinaryDecisionNode {
  public:
    AndDecisionNode(std::shared_ptr<DecisionNodeBase> iLeft, std::shared_ptr<DecisionNodeBase> iRight):
      BinaryDecisionNode(iLeft, iRight) {}
    virtual ~AndDecisionNode() final = default;
    private:
      ControlFlowStatus fullDecisionLogic_(ControlFlowStatus left, ControlFlowStatus right) const final;
      ControlFlowStatus shortCircuitLogic_(ControlFlowStatus decision) const final;
  };
}  // namespace edm
#endif //ControlFlow_AndDecisionNode_h