#include "TransitionHandling/FilesProcessor.h"
#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "Concurrency/chain_first.h"

namespace edm {
  //NOTE: distributor should be in pause state already
  void FilesProcessor::processFileTransitionAsync(edm::SourceCoordinator& coordinator,
                                                  edm::WaitingTaskHolder finalTask,
                                                  edm::WaitingTaskHolder nextTask) {
    using namespace edm::waiting_task;
    fileTransitionResource_.reset();
    queue_.push(*nextTask.group(),
                [this, &coordinator, holder = std::move(nextTask), finalTask = std::move(finalTask)]() mutable {
                  announceNewFileComing(std::move(finalTask));
                  queue_.pause();
                  //std::cout << "file transition processed" << std::endl;
                  //Would call any file handling here
                  coordinator.goToNextTransitionAsync(std::move(holder));
                });
  }

  void FilesProcessor::announceNewFileComing(edm::WaitingTaskHolder finalTask) {
    using namespace edm::waiting_task;
    auto task = chain::first([this](edm::WaitingTaskHolder holder) mutable { endFileAsync(std::move(holder)); }) |
                chain::then([this](std::exception_ptr const* ptr, edm::WaitingTaskHolder holder) mutable {
                  queue_.resume();
                  if (ptr) {
                    holder.doneWaiting(*ptr);
                    return;
                  }
                  holder.doneWaiting(std::exception_ptr{});
                }) |
                chain::lastTask(finalTask);
    fileTransitionResource_ = std::make_shared<FileTransitionResource>(std::move(task));
    for (auto* scheduler : dependentSchedulers_) {
      scheduler->newFileComing(fileTransitionResource_);
    }
  }

  void FilesProcessor::endFileAsync(edm::WaitingTaskHolder holder) {
    // Simulate ending the file transition here
    //std::cout << "ending file transition" << std::endl;
    holder.doneWaiting(std::exception_ptr{});
  }
}
