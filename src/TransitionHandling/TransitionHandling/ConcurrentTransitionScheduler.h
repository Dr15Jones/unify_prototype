#ifndef TransitionHandling_ConcurrentTransitionScheduler_h
#define TransitionHandling_ConcurrentTransitionScheduler_h

#include <vector>
#include <unordered_map>
#include <memory>
#include "Concurrency/IndexedLimitedTaskQueue.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/TransitionRecordKeyHash.h"
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
      : transition_(transitionKey), queue_(iNQueues), streamRecords_(iNQueues), streamHeldResources_(iNQueues) {}
  void addDependentScheduler(ConcurrentTransitionScheduler& scheduler) {
    dependentSchedulers_.push_back(&scheduler);
    scheduler.dependentUponTransition(transition_);
  }
  void dependentUponTransition(edm::TransitionRecordKey transitionKey) {
    dataDependentTransitions_.emplace(transitionKey, std::shared_ptr<ConcurrentTransitionResource>());
    for (auto& resource : streamHeldResources_) {
      resource.resize(dataDependentTransitions_.size());
    }
  }

  void addBeginAction(std::unique_ptr<AsyncActionBase> action) { beginActions_.push_back(std::move(action)); }
  void addEndAction(std::unique_ptr<AsyncActionBase> action) { endActions_.push_back(std::move(action)); }

  //Must only be called by TransitionsDistributor (as it serializes the calls to readAsync and tryToMergeAsync)
  void doneProcessing();
  void readAsync(edm::SourceCoordinator& coordinator,
                 edm::TransitionRecordID const& recordID,
                 TransitionsDistributorGuard&& distributor,
                 edm::WaitingTaskHolder holder);
 
  //called while TransitionsDistributor is still paused, so we don't have to worry about synchronization here.
  void newDataDependentTransitionComing(edm::TransitionRecordKey transitionKey,
                                        std::shared_ptr<ConcurrentTransitionResource> resource);

  void newFileComing(std::weak_ptr<FileTransitionResource> resource);

  void tryToMergeAsync(edm::SourceCoordinator& coordinator,
                       edm::TransitionRecordID const& recordID,
                       edm::WaitingTaskHolder holder);
 
private:
  //called only when TransitionDistributor is paused, so we don't have to worry about synchronization here.
  void holdResources(edm::ConcurrentTransitionID id);
  void releaseResources(edm::ConcurrentTransitionID id);
  //Called while the TransitionDistrobutor is still paused, so we don't have to worry about synchronization here.
  void announceNewTransitionComing(edm::ConcurrentTransitionID index,
                                   edm::TransitionRecordID const& recordID,
                                   edm::IndexedLimitedTaskQueue::Resumer resumer,
                                   edm::WaitingTaskHolder holder);
  void beginGlobalAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder);
  void beginDependentTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskHolder);
  void endDependentTransitionAsync(edm::TransitionRecordKey, edm::WaitingTaskHolder);
  void endGlobalAsync(edm::ConcurrentTransitionID stream, edm::WaitingTaskHolder holder);
 
  edm::TransitionRecordKey transition_;
  edm::IndexedLimitedTaskQueue queue_;
  std::shared_ptr<ConcurrentTransitionResource> transitionResource_;
  std::vector<ConcurrentTransitionScheduler*> dependentSchedulers_;
  //this is only modified or read while the TransitionDistributor is paused, so we don't have to worry about synchronization here.
  std::unordered_map<edm::TransitionRecordKey, std::shared_ptr<ConcurrentTransitionResource>, edm::TransitionRecordKeyHash>
      dataDependentTransitions_;
  std::weak_ptr<FileTransitionResource> fileTransitionResource_;
  std::vector<edm::TransitionRecordID> streamRecords_;
  std::vector<std::vector<std::shared_ptr<ConcurrentTransitionResource>>> streamHeldResources_;
  std::vector<std::unique_ptr<AsyncActionBase>> beginActions_;
  std::vector<std::unique_ptr<AsyncActionBase>> endActions_;
};

}

#endif