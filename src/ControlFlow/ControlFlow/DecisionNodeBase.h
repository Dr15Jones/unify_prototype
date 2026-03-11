#ifndef TransitionHandling_DecisionNodeBase_h
#define TransitionHandling_DecisionNodeBase_h
/*
Base class for nodes in the decision tree
*/
#include <vector>
#include <atomic>

#include "ControlFlow/ControlFlowStatus.h"
#include "ControlFlow/DecisionRequestState.h"

namespace edm {
  class DecisionRequestorBase;
  class WaitingTaskHolder;
  class TransitionProcessingContext;

  enum class RunStatus {
    NOT_STARTED,
    IN_PROGRESS,
    COMPLETED,
    WILL_NOT_RUN,
    SHOULD_RUN
  };

  class DecisionNodeBase {
  public:
    DecisionNodeBase():
    runStatus_{RunStatus::NOT_STARTED} {}
    virtual ~DecisionNodeBase() = default;

    void addRequestorForDecision(DecisionRequestorBase* iReq) { requestors_.push_back(iReq); }
    void requestDecisionAsync(WaitingTaskHolder iTask, TransitionProcessingContext & iContext, RequestState state);
    void resetForNewTransition() {
      if (requestors_.empty()) {
        runStatus_ = RunStatus::SHOULD_RUN;
      } else {
        runStatus_ = RunStatus::NOT_STARTED;
      }
      willNotRunRequests_.store(0, std::memory_order_release);
    }

    bool makeDecisionWillBeCalled() const {
      return !requestors_.empty();
    }

  protected:
    void sendDecisionToRequestorsAsync_(WaitingTaskHolder task,
                                        TransitionProcessingContext & context,
                                        ControlFlowStatus decision);
    void setInProgressForDiagnostics() { runStatus_ = RunStatus::IN_PROGRESS; }
    void setCompletedForDiagnostics() { runStatus_ = RunStatus::COMPLETED; }
    RunStatus runStatus() const { return runStatus_; }
  private:
    virtual void makeDecisionAsync_(WaitingTaskHolder task, TransitionProcessingContext & context, RequestState state) = 0;

    RunStatus runStatus_;
    std::vector<DecisionRequestorBase*> requestors_;
    std::atomic<unsigned int> willNotRunRequests_ = 0;
  };
}

#endif 