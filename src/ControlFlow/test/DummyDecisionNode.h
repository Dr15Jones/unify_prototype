#ifndef ControlFlow_DummyDecisionNode_h
#define ControlFlow_DummyDecisionNode_h

#include "ControlFlow/DecisionNodeBase.h"

namespace edm {
  class DummyDecisionNode final : public DecisionNodeBase {
  public:
    DummyDecisionNode(ControlFlowStatus status) : status_(status) {}
    ~DummyDecisionNode() = default;

  private:
    void makeDecisionAsync_(WaitingTaskHolder task, TransitionProcessingContext & context, RequestState state) final {
        if (state == RequestState::NO_REQUEST_COMING) {
          return;
        }
        task.group()->run([this, task = std::move(task), &context]() {
            setInProgressForDiagnostics();
            this->sendDecisionToRequestorsAsync_(std::move(task), context, this->status_);
        });
    }
    ControlFlowStatus status_;
  };
}  // namespace edm

#endif  // ControlFlow_DummyDecisionNode_h