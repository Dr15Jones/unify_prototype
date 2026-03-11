#ifndef TransitionHandling_DecisionRequestorBase_h
#define TransitionHandling_DecisionRequestorBase_h

#include "ControlFlow/ControlFlowStatus.h"

namespace edm {
  class WaitingTaskHolder;
  class TransitionProcessingContext;
  class DecisionRequestorBase {
  public:
    virtual void decisionFromNodeAsync(WaitingTaskHolder, TransitionProcessingContext &, void const*, ControlFlowStatus) = 0;
  };
}  // namespace edm

#endif  //TransitionHandling_DecisionRequestorBase_h