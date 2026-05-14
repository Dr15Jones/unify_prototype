#ifndef TransitionHandling_FilesProcessor_h
#define TransitionHandling_FilesProcessor_h

#include <memory>
#include <vector>
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/FileTransitionResource.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/SerialTaskQueue.h"

namespace edm {
  class ConcurrentTransitionScheduler;

class FilesProcessor {
public:
  void addDependentScheduler(ConcurrentTransitionScheduler& scheduler) { dependentSchedulers_.push_back(&scheduler); }
  //NOTE: distributor should be in pause state already
  void processFileTransitionAsync(edm::SourceCoordinator& coordinator,
                                  edm::WaitingTaskHolder finalTask,
                                  edm::WaitingTaskHolder nextTask);
 
  void doneProcessing() { fileTransitionResource_.reset(); }

private:
  void announceNewFileComing(edm::WaitingTaskHolder finalTask);

  void endFileAsync(edm::WaitingTaskHolder holder);
  edm::SerialTaskQueue queue_;
  //Should I have the StreamSchedulers hold weak_ptrs to the FileTransitionResource and then
  // convert them to shared_ptr when they need to do begin processing? That should automatically
  // release the resource when the last StreamScheduler is done with it.
  std::shared_ptr<FileTransitionResource> fileTransitionResource_;
  std::vector<ConcurrentTransitionScheduler*> dependentSchedulers_;
};
}  // namespace edm

#endif