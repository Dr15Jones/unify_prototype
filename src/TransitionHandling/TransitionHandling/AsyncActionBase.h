#ifndef TransitionHandling_AsyncActionBase_h
#define TransitionHandling_AsyncActionBase_h
/*
Base class for actions that can be taken at the beginning or end of a transition.
*/

#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  class TransitionRecordKey;
  class TransitionRecordID;
  class ConcurrentTransitionID;

  class AsyncActionBase {
  public:
    AsyncActionBase() = default;
    virtual ~AsyncActionBase() = default;

    //temp form
    virtual void performAsync(edm::WaitingTaskHolder,
                              edm::TransitionRecordKey const&,
                              edm::ConcurrentTransitionID,
                              edm::TransitionRecordID const&) = 0;
    /*
    //Both TransitionContext and SchedulingResults have lifetimes longer than the asynchronous operation.
    virtual void performAsync(WaitingTaskHolder, TransitionContext&, SchedulingResults&) = 0;
    */
  };
}  // namespace edm

#endif