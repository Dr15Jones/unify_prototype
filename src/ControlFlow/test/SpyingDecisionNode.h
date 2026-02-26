#ifndef ControlFlow_SpyingDecisionNode_h
#define ControlFlow_SpyingDecisionNode_h

#include "ControlFlow/DecisionRequestorBase.h"
#include "ControlFlow/DecisionNodeBase.h"
#include "Concurrency/WaitingTaskHolder.h"

#include <memory>

namespace edm {
  class SpyingDecisionNode : public DecisionNodeBase, public DecisionRequestorBase {
  public:
    SpyingDecisionNode(std::shared_ptr<DecisionNodeBase> iNode ) : node_{iNode}, status_(ControlFlowStatus::NOT_STARTED) {
            node_->addRequestorForDecision(this);
    }
    ~SpyingDecisionNode() final = default;

    void decisionFromNodeAsync(WaitingTaskHolder task,
                                        TransitionProcessingContext const & context,
                                        void const* nodeHash,
                                        ControlFlowStatus decision) final {
        status_ = decision;
        sendDecisionToRequestorsAsync_(task, context, decision);
    }

    ControlFlowStatus status() const { return status_; }
  private:
    void makeDecisionAsync_(WaitingTaskHolder task, TransitionProcessingContext const & context, RequestState state) final {
        if (state != RequestState::NO_REQUEST_COMING) {
          setInProgressForDiagnostics();
        }
        node_->requestDecisionAsync(task, context, state);
    }
    std::shared_ptr<DecisionNodeBase> node_;
    ControlFlowStatus status_;
  };
}  // namespace edm

#endif  // ControlFlow_SpyingDecisionNode_h