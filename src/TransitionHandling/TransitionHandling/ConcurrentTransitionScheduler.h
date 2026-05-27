#ifndef TransitionHandling_ConcurrentTransitionScheduler_h
#define TransitionHandling_ConcurrentTransitionScheduler_h

#include <vector>
#include <unordered_map>
#include <memory>
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/WaitingTaskList.h"
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/TransitionRecordKeyHash.h"
#include "TransitionHandling/ConcurrentTransitionsTaskQueue.h"
#include "TransitionHandling/TransitionRecordID.h"
#include "TransitionHandling/ConcurrentTransitionID.h"
#include "TransitionHandling/AsyncActionBase.h"

namespace edm {
  class SourceCoordinator;
  class TransitionsDistributorGuard;
  class FileTransitionResource;
  /* A stream goes through the phases
- read: initializes the Transition record
- begin: runs the begin transitions for registered components
- available: the transition record is available for other stream's processing
- end: runs once all other streams are done with this transition
*/
  class ConcurrentTransitionResource {
  public:
    edm::ConcurrentTransitionID replica_{0};
    edm::TransitionRecordID recordID_;
    edm::WaitingTaskHolder holder_;
  };

  class ConcurrentTransitionScheduler {
  public:
    ConcurrentTransitionScheduler(edm::TransitionRecordKey transitionKey, unsigned int iNQueues)
        : transition_(transitionKey),
          queue_(iNQueues),
          waitingDependentTransitionTasks_(iNQueues),
          concurrentRecords_(iNQueues),
          concurrentHeldSupporterResources_(iNQueues) {}
    void addDependentScheduler(ConcurrentTransitionScheduler& scheduler) {
      dependentSchedulers_.push_back(&scheduler);
      scheduler.supporterTransition(transition_);
      for (auto& [key, resource] : supporterResources_) {
        scheduler.supporterTransition(key);
      }
    }
    void supporterTransition(edm::TransitionRecordKey transitionKey) {
      supporterResources_.emplace(transitionKey, std::shared_ptr<ConcurrentTransitionResource>());
      for (auto& resource : concurrentHeldSupporterResources_) {
        resource.resize(supporterResources_.size());
      }
      for (auto* scheduler : dependentSchedulers_) {
        scheduler->supporterTransition(transitionKey);
      }
    }

    void addBeginAction(std::unique_ptr<AsyncActionBase> action) { beginActions_.push_back(std::move(action)); }
    void addEndAction(std::unique_ptr<AsyncActionBase> action) { endActions_.push_back(std::move(action)); }
    void addSupporterBeginAction(edm::TransitionRecordKey key, std::unique_ptr<AsyncActionBase> action) {
      supporterBeginActions_[key].push_back(std::move(action));
    }
    void addSupporterEndAction(edm::TransitionRecordKey key, std::unique_ptr<AsyncActionBase> action) {
      supporterEndActions_[key].push_back(std::move(action));
    }

    //Must only be called by TransitionsDistributor (as it serializes the calls to readAsync and tryToMergeAsync)
    void doneProcessing();
    void readAsync(edm::SourceCoordinator& coordinator,
                   edm::TransitionRecordID const& recordID,
                   TransitionsDistributorGuard&& distributor,
                   edm::WaitingTaskHolder holder);

    //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
    void newSupporterTransitionComing(edm::TransitionRecordKey transitionKey);
    void newSupporterTransitionResource(edm::TransitionRecordKey transitionKey,
                                        std::shared_ptr<ConcurrentTransitionResource> resource);
    void newSupporterTransitionAvailable(edm::TransitionRecordKey transitionKey,
                                         edm::WaitingTaskList& waitingTasks,
                                         edm::WaitingTaskHolder holder);

    void newFileComing(std::weak_ptr<FileTransitionResource const> resource);

    void tryToMergeAsync(edm::SourceCoordinator& coordinator,
                         edm::TransitionRecordID const& recordID,
                         edm::WaitingTaskHolder holder);

  private:
    //called only when TransitionDistributor is paused, so we don't have to worry about synchronization here.
    void holdResources(edm::ConcurrentTransitionID id);
    void releaseResources(edm::ConcurrentTransitionID id);
    //Called while the TransitionDistributor is still paused, so we don't have to worry about synchronization here.
    void announceNewTransitionComing(edm::ConcurrentTransitionID index,
                                     edm::TransitionRecordID const& recordID,
                                     edm::ConcurrentTransitionsTaskQueue::Resumer resumer,
                                     edm::WaitingTaskHolder holder);
    void announceNewTransitionAvailable(edm::ConcurrentTransitionID index, edm::WaitingTaskHolder holder);
    void processBeginAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder);
    void beginSupporterTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskList&, edm::WaitingTaskHolder);
    void processBeginSupporterTransitionAsync(ConcurrentTransitionID stream,
                                              edm::TransitionRecordKey const&,
                                              edm::WaitingTaskHolder);
    void endSupporterTransitionAsync(edm::TransitionRecordKey const&,
                                     edm::TransitionRecordID const&,
                                     edm::WaitingTaskHolder);
    void processEndSupporterTransitionAsync(ConcurrentTransitionID stream,
                                            edm::TransitionRecordKey const&,
                                            edm::TransitionRecordID const&,
                                            edm::WaitingTaskHolder);
    void processEndAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder);

    edm::TransitionRecordKey transition_;
    //Handles making sure all the streams agree on the ordering of dependent transitions.
    edm::ConcurrentTransitionsTaskQueue queue_;
    //the most recently announced transition record for this transition, used to merge the first file open with the read if possible. This is only accessed while the TransitionDistributor is paused, so we don't have to worry about synchronization here.
    std::shared_ptr<ConcurrentTransitionResource> transitionResource_;
    //the schedulers for the transitions that depend on this transition. Used to inform them of a new transition.
    std::vector<ConcurrentTransitionScheduler*> dependentSchedulers_;
    //Cache of the resources for the supporter transitions. Keeps those transitions open until we are no longer processing a transition which depends on them.
    //this is only modified or read while the TransitionDistributor is paused, so we don't have to worry about synchronization here.
    std::unordered_map<edm::TransitionRecordKey,
                       std::shared_ptr<ConcurrentTransitionResource>,
                       edm::TransitionRecordKeyHash>
        supporterResources_;
    //used to keep the file resource alive only until the transition has finished its begin process.
    std::weak_ptr<FileTransitionResource const> fileTransitionResource_;
    //For each stream (index) holds the waiting tasks for the dependent transitions. The tasks are informed once the transition record is available and can then run the dependent transitions when they are scheduled to run.
    std::vector<edm::WaitingTaskList> waitingDependentTransitionTasks_;
    std::vector<edm::TransitionRecordID> concurrentRecords_;
    //For each stream (first index) and each data dependent transition (second index) holds the resource for that transition while the stream is processing it. This allows us to release all resources for a stream at once when it finishes processing the transition.
    std::vector<std::vector<std::shared_ptr<ConcurrentTransitionResource>>> concurrentHeldSupporterResources_;
    std::vector<std::unique_ptr<AsyncActionBase>> beginActions_;
    std::vector<std::unique_ptr<AsyncActionBase>> endActions_;
    std::unordered_map<edm::TransitionRecordKey,
                       std::vector<std::unique_ptr<AsyncActionBase>>,
                       edm::TransitionRecordKeyHash>
        supporterBeginActions_;
    std::unordered_map<edm::TransitionRecordKey,
                       std::vector<std::unique_ptr<AsyncActionBase>>,
                       edm::TransitionRecordKeyHash>
        supporterEndActions_;
  };

}  // namespace edm

#endif