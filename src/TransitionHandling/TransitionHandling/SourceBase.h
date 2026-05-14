#ifndef TransitionHandling_SourceBase_h
#define TransitionHandling_SourceBase_h
/*
Base class for the source of transitions. This is used by the SourceCoordinator to peek the next transition and to read transitions. The source is not thread safe, so all access to the source must be serialized by the SourceCoordinator.
*/

#include "TransitionHandling/SourcePeekResult.h"
#include "TransitionHandling/ConcurrentTransitionID.h"

namespace edm {
  class SourceBase {
  public:
    virtual ~SourceBase() = default;
    virtual void readTransition(edm::TransitionRecordKey transitionKey, edm::ConcurrentTransitionID) = 0;
    virtual void mergeTransition(edm::TransitionRecordKey transitionKey,
                                 edm::TransitionRecordID const& recordID,
                                 edm::ConcurrentTransitionID) = 0;
    virtual SourcePeekResult goToNextTransition() = 0;
  };
}  // namespace edm

#endif