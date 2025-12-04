#ifndef TransitionHandling_DecisionRequestorBase_h
#define TransitionHandling_DecisionRequestorBase_h

#include "ControlFlow/ControlFlowStatus.h"

namespace edm {
  class WaitingTaskHolder;
  class TransitionContext;
  class DecisionRequestorBase {
  public:
    virtual void decisionFromNodeAsync(WaitingTaskHolder, TransitionContext&, void const*, ControlFlowStatus) = 0;
  };
}  // namespace edm

#endif  //TransitionHandling_DecisionRequestorBase_h