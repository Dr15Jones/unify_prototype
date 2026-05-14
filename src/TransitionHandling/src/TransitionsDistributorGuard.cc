#include "TransitionHandling/TransitionsDistributorGuard.h"
#include "TransitionHandling/TransitionsDistributor.h"

namespace edm {
  TransitionsDistributorGuard::~TransitionsDistributorGuard() noexcept {
    if (distributor_) {
      distributor_->failedDuringRead();
      //do I want to trigger the next transition here if there was a failure?
    }
  }
  void TransitionsDistributorGuard::release() noexcept {
    if (distributor_) {
      distributor_->distributeNextTransitionAsync(std::move(holder_));
      distributor_ = nullptr;
    }
  }
}  // namespace edm