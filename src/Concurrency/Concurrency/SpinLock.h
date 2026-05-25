#ifndef Concurrency_SpinLock_h
#define Concurrency_SpinLock_h
// -*- C++ -*-
// Package:     Concurrency
// Class  :     SpinLock
/// Description: A simple spin lock implementation.
// Original Author:  Chris Jones

#include <atomic>

namespace edm {
  class SpinLock {
  public:
    void lock() noexcept {
      while (flag_.test_and_set(std::memory_order_acquire)) {
      }
    }

    void unlock() noexcept { flag_.clear(std::memory_order_release); }

    bool isLocked() noexcept { return flag_.test(std::memory_order_acquire); }
  private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
  };
}  // namespace edm
#endif