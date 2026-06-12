#ifndef TransitionHandling_TransitionsDistributor_h
#define TransitionHandling_TransitionsDistributor_h

#include "DataModel/TransitionRecordKey.h"
#include "DataModel/TransitionRecordKeyHash.h"
#include "DataModel/TransitionRecordID.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/SourcePeekResult.h"
#include "TransitionHandling/FilesProcessor.h"
#include "Concurrency/WaitingTaskHolder.h"

#include <optional>
#include <unordered_map>
#include <atomic>

namespace edm {
  class ConcurrentTransitionScheduler;
  //peeks at the source and then sends the transition to the proper StreamScheduler
  class TransitionsDistributor {
  public:
    friend class TransitionsDistributorGuard;

    TransitionsDistributor(edm::SourceCoordinator& coordinator) : coordinator_(coordinator) {}
    void addSchedulerForTransition(edm::TransitionRecordKey transitionKey, ConcurrentTransitionScheduler& scheduler);

    void processData();

    void failedDuringRead() { failureDuringRead_ = true; }

  private:
    void distributeNextTransitionAsync(edm::WaitingTaskHolder holder);

    void tryToMergeAfterNewFileAsync(std::optional<edm::TransitionRecordID> recordID, edm::WaitingTaskHolder holder);
    edm::SourceCoordinator& coordinator_;
    FilesProcessor filesProcessor_;
    std::optional<edm::SourcePeekResult> nextTransition_;
    std::unordered_map<edm::TransitionRecordKey, ConcurrentTransitionScheduler*, edm::TransitionRecordKeyHash> schedulers_;
    std::atomic<bool> failureDuringRead_{false};
    std::atomic<bool> failureDuringProcessing_{false};
  };
}  // namespace edm

#endif // TransitionHandling_TransitionsDistributor_h