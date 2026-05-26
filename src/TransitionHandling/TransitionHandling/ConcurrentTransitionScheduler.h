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
      : transition_(transitionKey), queue_(iNQueues), waitingDependentTransitionTasks_(iNQueues), concurrentRecords_(iNQueues), concurrentHeldResources_(iNQueues) {}
  void addDependentScheduler(ConcurrentTransitionScheduler& scheduler) {
    dependentSchedulers_.push_back(&scheduler);
    scheduler.dependentUponTransition(transition_);
    for(auto& [key, resource] : dataDependentTransitions_) {
      scheduler.dependentUponTransition(key);
    }
  }
  void dependentUponTransition(edm::TransitionRecordKey transitionKey) {
    dataDependentTransitions_.emplace(transitionKey, std::shared_ptr<ConcurrentTransitionResource>());
    for (auto& resource : concurrentHeldResources_) {
      resource.resize(dataDependentTransitions_.size());
    }
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->dependentUponTransition(transitionKey);
    }
  }

  void addBeginAction(std::unique_ptr<AsyncActionBase> action) { beginActions_.push_back(std::move(action)); }
  void addEndAction(std::unique_ptr<AsyncActionBase> action) { endActions_.push_back(std::move(action)); }
  void addDataDependentBeginAction(edm::TransitionRecordKey key, std::unique_ptr<AsyncActionBase> action) {
    dependentBeginActions_[key].push_back(std::move(action));
  }
  void addDataDependentEndAction(edm::TransitionRecordKey key, std::unique_ptr<AsyncActionBase> action) { dependentEndActions_[key].push_back(std::move(action)); }

  //Must only be called by TransitionsDistributor (as it serializes the calls to readAsync and tryToMergeAsync)
  void doneProcessing();
  void readAsync(edm::SourceCoordinator& coordinator,
                 edm::TransitionRecordID const& recordID,
                 TransitionsDistributorGuard&& distributor,
                 edm::WaitingTaskHolder holder);
 
  //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
  void newDataDependentTransitionComing(edm::TransitionRecordKey transitionKey);
  void newDataDependentTransitionResource(edm::TransitionRecordKey transitionKey,
                                        std::shared_ptr<ConcurrentTransitionResource> resource);
  void newDataDependentTransitionAvailable(edm::TransitionRecordKey transitionKey, edm::WaitingTaskList& waitingTasks, edm::WaitingTaskHolder holder);

  void newFileComing(std::weak_ptr<FileTransitionResource> resource);

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
  void beginDependentTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskList&, edm::WaitingTaskHolder);
  void processBeginDependentTransitionAsync(ConcurrentTransitionID stream, edm::TransitionRecordKey const&, edm::WaitingTaskHolder);
  void endDependentTransitionAsync(edm::TransitionRecordKey const&,  edm::TransitionRecordID const&, edm::WaitingTaskHolder);
  void processEndDependentTransitionAsync(ConcurrentTransitionID stream, edm::TransitionRecordKey const&, edm::TransitionRecordID const&, edm::WaitingTaskHolder);
  void processEndAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder);
 
  edm::TransitionRecordKey transition_;
  edm::ConcurrentTransitionsTaskQueue queue_;
  std::shared_ptr<ConcurrentTransitionResource> transitionResource_;
  std::vector<ConcurrentTransitionScheduler*> dependentSchedulers_;
  //this is only modified or read while the TransitionDistributor is paused, so we don't have to worry about synchronization here.
  std::unordered_map<edm::TransitionRecordKey, std::shared_ptr<ConcurrentTransitionResource>, edm::TransitionRecordKeyHash>
      dataDependentTransitions_;
  std::weak_ptr<FileTransitionResource> fileTransitionResource_;
  std::vector<edm::WaitingTaskList> waitingDependentTransitionTasks_;
  std::vector<edm::TransitionRecordID> concurrentRecords_;
  std::vector<std::vector<std::shared_ptr<ConcurrentTransitionResource>>> concurrentHeldResources_;
  std::vector<std::unique_ptr<AsyncActionBase>> beginActions_;
  std::vector<std::unique_ptr<AsyncActionBase>> endActions_;
  std::unordered_map<edm::TransitionRecordKey, std::vector<std::unique_ptr<AsyncActionBase>>, edm::TransitionRecordKeyHash> dependentBeginActions_;
  std::unordered_map<edm::TransitionRecordKey, std::vector<std::unique_ptr<AsyncActionBase>>, edm::TransitionRecordKeyHash> dependentEndActions_;
};

}

#endif