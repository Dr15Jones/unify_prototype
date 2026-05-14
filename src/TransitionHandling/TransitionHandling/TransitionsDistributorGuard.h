#ifndef TransitionHandling_TransitionsDistributorGuard_h
#define TransitionHandling_TransitionsDistributorGuard_h
/* TransitionsDistributorGuard
   1. holds a WaitingTaskHolder which will be used to trigger the next transition distribution when release is called
    2. ensures that if the guard goes out of scope without release being called, TransitionsDistributor::failedDuringRead is called which will stop the processing loop.
    3. provides access to the final task which is used by end transition calls to be sure those end transitions happen before ending the processing loop.
*/

#include "Concurrency/WaitingTaskHolder.h"

namespace edm {
  class TransitionsDistributor;

  class TransitionsDistributorGuard {
  public:
    TransitionsDistributorGuard(TransitionsDistributor& distributor, edm::WaitingTaskHolder holder) noexcept
        : distributor_(&distributor), holder_(std::move(holder)) {}
    ~TransitionsDistributorGuard() noexcept;
    TransitionsDistributorGuard(TransitionsDistributorGuard&& iOther) noexcept
        : distributor_(iOther.distributor_), holder_(std::move(iOther.holder_)) {
      iOther.distributor_ = nullptr;
    }
    TransitionsDistributorGuard(const TransitionsDistributorGuard&) = delete;
    TransitionsDistributorGuard& operator=(const TransitionsDistributorGuard&) = delete;
    TransitionsDistributorGuard& operator=(TransitionsDistributorGuard&& iOther) noexcept {
      if (this != &iOther) {
        distributor_ = iOther.distributor_;
        iOther.distributor_ = nullptr;
        holder_ = std::move(iOther.holder_);
      }
      return *this;
    }
    void release() noexcept;

    edm::WaitingTaskHolder finalTask() const noexcept { return holder_; }

  private:
    TransitionsDistributor* distributor_;
    edm::WaitingTaskHolder holder_;
  };

}  // namespace edm

#endif