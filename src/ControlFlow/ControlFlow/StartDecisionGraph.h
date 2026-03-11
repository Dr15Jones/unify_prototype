#ifndef ControlFlow_StartDecisionGraph_h
#define ControlFlow_StartDecisionGraph_h

#include "ControlFlow/DecisionRequestorBase.h"
#include <atomic>

namespace edm {
  class DecisionNodeBase;
  class WaitingTaskHolder;
  class TransitionProcessingContext;

  class StartDecisionGraph : public DecisionRequestorBase {
  public:
    StartDecisionGraph() = default;
    virtual ~StartDecisionGraph() final = default;

    void startAsync(WaitingTaskHolder task, TransitionProcessingContext & context);

    void addLeafNode(DecisionNodeBase* iLeaf);

    void decisionFromNodeAsync(WaitingTaskHolder task,
                               TransitionProcessingContext & context,
                               void const* nodeHash,
                               ControlFlowStatus decision) final;

  private:
    std::atomic<unsigned int> finishedLeafNodes_ = 0;
    std::vector<DecisionNodeBase*> leafNodes_;
  };
}  // namespace edm
#endif  //ControlFlow_StartDecisionGraph_h