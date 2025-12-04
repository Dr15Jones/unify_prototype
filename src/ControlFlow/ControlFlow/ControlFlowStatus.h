#ifndef TransitionHandling_ControlFlowStatus_h
#define TransitionHandling_ControlFlowStatus_h

namespace edm {
  enum class ControlFlowStatus {
    NOT_STARTED,
    GO,
    SKIP,
    EXCEPTION
  };
}

#endif //TransitionHandling_ControlFlowStatus_h