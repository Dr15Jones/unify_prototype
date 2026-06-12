#ifndef TransitionHandling_SourcePeekResult_h
#define TransitionHandling_SourcePeekResult_h
/*
Class to represent the result of peeking the next transition from the source. This is used by the SourceCoordinator to determine which scheduler to run and which actions to take at the beginning and end of a transition.
*/
#include <optional>
#include "TransitionHandling/SourceNextState.h"
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/TransitionRecordID.h"

namespace edm {
  class SourcePeekResult {
  public:
    SourcePeekResult(SourceNextState state,
                     std::optional<TransitionRecordKey> recordKey = std::nullopt,
                     std::optional<TransitionRecordID> recordID = std::nullopt)
        : state_(state), recordKey_(recordKey), recordID_(recordID) {}

    SourceNextState state() const { return state_; }
    std::optional<TransitionRecordKey> recordKey() const { return recordKey_; }
    std::optional<TransitionRecordID> recordID() const { return recordID_; }

  private:
    SourceNextState state_;
    std::optional<TransitionRecordKey> recordKey_;
    std::optional<TransitionRecordID> recordID_;
  };
}  // namespace edm
#endif

