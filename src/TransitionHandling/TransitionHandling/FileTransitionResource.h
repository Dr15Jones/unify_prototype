#ifndef TransitionHandling_FileTransitionResource_h
#define TransitionHandling_FileTransitionResource_h
#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
class FileTransitionResource {
public:
  FileTransitionResource(edm::WaitingTaskHolder holder) : holder_(std::move(holder)) {}

private:
  edm::WaitingTaskHolder holder_;
};
}  // namespace edm
#endif