#ifndef Concurrency_syncWait_h
#define Concurrency_syncWait_h
//
//  syncWait.h
//
//  Created by Chris Jones on 2/24/21.
//
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/FinalWaitingTask.h"
#include "oneapi/tbb/task_group.h"
#include <exception>

namespace edm {
  template <typename F>
  [[nodiscard]] std::exception_ptr syncWait(F&& iFunc) {
    std::exception_ptr exceptPtr{};
    oneapi::tbb::task_group group;
    FinalWaitingTask last{group};
    group.run([&]() { iFunc(WaitingTaskHolder(group, &last)); });  //group.run

    return last.waitNoThrow();
  }
}  // namespace edm
#endif /* Concurrency_syncWait_h */
