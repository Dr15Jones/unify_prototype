#ifndef Concurrency_IndexedLimitedTaskQueue_h
#define Concurrency_IndexedLimitedTaskQueue_h
// -*- C++ -*-
//
// Package:     Concurrency
// Class  :     IndexedIndexedLimitedTaskQueue
//
/**\class IndexedLimitedTaskQueue IndexedLimitedTaskQueue.h "Concurrency/IndexedLimitedTaskQueue.h"

 Description: Runs a set number of tasks from the queue at a time

 Usage:
    A IndexedLimitedTaskQueue is used to provide access to a limited thread-safe resource. You create a IndexedLimitedTaskQueue
 for the resource. When every you need to perform an operation on the resource, you push a 'task' that
 does that operation onto the queue. The queue then makes sure to run a limited number of tasks at a time.
 
    The 'tasks' managed by the IndexedLimitedTaskQueue are just functor objects who which take no arguments and
 return no values. The simplest way to create a task is to use a C++11 lambda.
 
*/
//
// Original Author:  Chris Jones
//         Created:  Thu Feb 21 11:14:39 CST 2013
// $Id$
//

// system include files
#include <atomic>
#include <vector>
#include <memory>

#include "Concurrency/SerialTaskQueue.h"
#include "Utilities/thread_safety_macros.h"

// user include files

// forward declarations
namespace edm {
  class IndexedLimitedTaskQueue {
  public:
    IndexedLimitedTaskQueue(unsigned int iLimit) : m_queues{iLimit} {}
    IndexedLimitedTaskQueue(const IndexedLimitedTaskQueue&) = delete;
    const IndexedLimitedTaskQueue& operator=(const IndexedLimitedTaskQueue&) = delete;

    // ---------- member functions ---------------------------

    /// asynchronously pushes functor iAction into queue
    /**
       * The function will return immediately and iAction will either
       * process concurrently with the calling thread or wait until the
       * protected resource becomes available or until a CPU becomes available.
       * \param[in] iAction Must be a functor that takes an size_t argument and return no values.
       */
    template <typename T>
      requires requires(T&& t) { t(size_t(0)); }
    void push(oneapi::tbb::task_group& iGroup, T&& iAction);

    template <typename T>
      requires requires(T&& t) { t(size_t(0)); }
    void pushToAll(oneapi::tbb::task_group& iGroup, T&& iAction);

    class Resumer {
    public:
      friend class IndexedLimitedTaskQueue;

      Resumer() = default;
      ~Resumer() { resume(); }

      Resumer(Resumer&& iOther) : m_queue(iOther.m_queue) { iOther.m_queue = nullptr; }

      Resumer(Resumer const& iOther) : m_queue(iOther.m_queue) {
        if (m_queue) {
          m_queue->pause();
        }
      }

      Resumer& operator=(Resumer const& iOther) {
        auto t = iOther;
        return (*this = std::move(t));
      }
      Resumer& operator=(Resumer&& iOther) {
        if (m_queue) {
          m_queue->resume();
        }
        m_queue = iOther.m_queue;
        iOther.m_queue = nullptr;
        return *this;
      }

      bool resume() {
        if (m_queue) {
          auto q = m_queue;
          m_queue = nullptr;
          return q->resume();
        }
        return false;
      }

    private:
      Resumer(SerialTaskQueue* iQueue) : m_queue{iQueue} {}
      SerialTaskQueue* m_queue = nullptr;
    };

    /// asynchronously pushes functor iAction into queue then pause the queue and run iAction
    /** iAction must take as argument a copy of a IndexedLimitedTaskQueue::Resumer. To resume
       the queue let the last copy of the Resumer go out of scope, or call Resumer::resume().
       Using this function will decrease the allowed concurrency limit by 1.
       */
    template <typename T>
      requires requires(T&& t, Resumer&& r, size_t s) { t(std::move(r), s); }
    void pushAndPause(oneapi::tbb::task_group& iGroup, T&& iAction);

    template <typename T>
      requires requires(T&& t, Resumer&& r, size_t s) { t(std::move(r), s); }
    void pushToAllAndPause(oneapi::tbb::task_group& iGroup, T&& iAction);

    unsigned int concurrencyLimit() const { return m_queues.size(); }

  private:
    // ---------- member data --------------------------------
    std::vector<SerialTaskQueue> m_queues;
  };

  template <typename T>
    requires requires(T&& t) { t(size_t(0)); }
  void IndexedLimitedTaskQueue::push(oneapi::tbb::task_group& iGroup, T&& iAction) {
    auto set_to_run = std::make_shared<std::atomic<bool>>(false);
    size_t index = 0;
    for (auto& q : m_queues) {
      q.push(iGroup, [set_to_run, index, iAction]() mutable {
        if (not set_to_run->exchange(true)) {
          iAction(index);
        }
      });
      ++index;
    }
  }

  template <typename T>
    requires requires(T&& t) { t(size_t(0)); }
  void IndexedLimitedTaskQueue::pushToAll(oneapi::tbb::task_group& iGroup, T&& iAction) {
    size_t index = 0;
    for (auto& q : m_queues) {
      q.push(iGroup, [index, iAction]() mutable { iAction(index); });
      ++index;
    }
  }

  template <typename T>
    requires requires(T&& t, IndexedLimitedTaskQueue::Resumer&& r, size_t s) { t(std::move(r), s); }
  void IndexedLimitedTaskQueue::pushAndPause(oneapi::tbb::task_group& iGroup, T&& iAction) {
    auto set_to_run = std::make_shared<std::atomic<bool>>(false);
    size_t index = 0;
    for (auto& q : m_queues) {
      q.push(iGroup, [&q, set_to_run, iAction, index]() mutable {
        if (not set_to_run->exchange(true)) {
          q.pause();
          iAction(Resumer(&q), index);
        }
      });
      ++index;
    }
  }

  template <typename T>
    requires requires(T&& t, IndexedLimitedTaskQueue::Resumer&& r, size_t s) { t(std::move(r), s); }
  void IndexedLimitedTaskQueue::pushToAllAndPause(oneapi::tbb::task_group& iGroup, T&& iAction) {
    size_t index = 0;
    for (auto& q : m_queues) {
      q.push(iGroup, [&q, iAction, index]() mutable {
        q.pause();
        iAction(Resumer(&q), index);
      });
      ++index;
    }
  }

}  // namespace edm

#endif
