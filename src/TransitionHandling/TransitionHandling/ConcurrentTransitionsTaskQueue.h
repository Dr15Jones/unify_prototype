#ifndef TransitionHandling_ConcurrentTransitionsTaskQueue_h
#define TransitionHandling_ConcurrentTransitionsTaskQueue_h
// -*- C++ -*-
//
// Package:     TransitionHandling
// Class  :     IndexedConcurrentTransitionsTaskQueue
//
/**\class ConcurrentTransitionsTaskQueue ConcurrentTransitionsTaskQueue.h "TransitionHandling/ConcurrentTransitionsTaskQueue.h"

 Description: Runs a set number of tasks from the queue at a time

 Usage:
    A ConcurrentTransitionsTaskQueue is used to provide access to a limited thread-safe resource. You create a ConcurrentTransitionsTaskQueue
 for the resource. When every you need to perform an operation on the resource, you push a 'task' that
 does that operation onto the queue. The queue then makes sure to run a limited number of tasks at a time.
 
    The 'tasks' managed by the ConcurrentTransitionsTaskQueue are just functor objects who which take no arguments and
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
#include <mutex>
#include <utility>
#include <oneapi/tbb/concurrent_queue.h>

#include "TransitionHandling/ConcurrentTransitionTaskQueue.h"
#include "Concurrency/SpinLock.h"
#include "Utilities/thread_safety_macros.h"
#include "Utilities/make_sentry.h"
#include "Utilities/Likely.h"

// user include files

// forward declarations
namespace edm {
  class ConcurrentTransitionsTaskQueue {
  public:
    friend class ConcurrentTransitionTaskQueue;

    ConcurrentTransitionsTaskQueue(unsigned int iLimit)
        : m_availableQueues{}, m_processing(std::make_shared<std::atomic<bool>>(false)) {
      m_availableQueues.set_capacity(iLimit);
      m_queues.reserve(iLimit);
      for (unsigned int i = 0; i < iLimit; ++i) {
        m_queues.emplace_back(*this, i, m_processing);
      }
    }
    ConcurrentTransitionsTaskQueue(const ConcurrentTransitionsTaskQueue&) = delete;
    const ConcurrentTransitionsTaskQueue& operator=(const ConcurrentTransitionsTaskQueue&) = delete;

    // ---------- member functions ---------------------------

    void pauseAll() {
      for (auto& q : m_queues) {
        q.pause();
      }
    }
    void resumeAll() {
      for (auto& q : m_queues) {
        q.resume();
      }
    }
    class Resumer {
    public:
      friend class ConcurrentTransitionsTaskQueue;

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
      Resumer(ConcurrentTransitionTaskQueue* iQueue) : m_queue{iQueue} {}
      ConcurrentTransitionTaskQueue* m_queue = nullptr;
    };

    /// asynchronously pushes functor iAction into queue then pause the queue and run iAction
    /** iAction must take as argument a copy of a ConcurrentTransitionsTaskQueue::Resumer. To resume
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
    bool tryFromSharedQueue(ConcurrentTransitionTaskBase*& oTask,
                            std::size_t iQueueIndex,
                            std::atomic<bool>& iProcessing) {
      if (iProcessing.load()) {
        //Need to have sole access to m_sharedTask and m_availableQueues to avoid race conditions
        std::lock_guard<edm::SpinLock> lock{m_sharedTaskAndQueue};
        if (m_sharedTask) {
          oTask = std::exchange(m_sharedTask, nullptr);
          return true;
        } else {
          [[maybe_unused]] auto succeeded = m_availableQueues.try_push(iQueueIndex);
          assert(succeeded);
        }
      }
      return false;
    }
    // ---------- member data --------------------------------
    std::vector<ConcurrentTransitionTaskQueue> m_queues;
    oneapi::tbb::concurrent_bounded_queue<size_t> m_availableQueues;
    ConcurrentTransitionTaskBase* m_sharedTask = nullptr;  //guarded by m_sharedTaskAndQueue
    std::shared_ptr<std::atomic<bool>> m_processing;
    edm::SpinLock m_sharedTaskAndQueue;
  };

  template <typename T>
    requires requires(T&& t, ConcurrentTransitionsTaskQueue::Resumer&& r, size_t s) { t(std::move(r), s); }
  void ConcurrentTransitionsTaskQueue::pushAndPause(oneapi::tbb::task_group& iGroup, T&& iAction) {
    if (not m_processing->load()) {
      m_processing = std::make_shared<std::atomic<bool>>(true);
      for (auto& q : m_queues) {
        q.push(iGroup, [&q, this](std::size_t index) { q.processing(m_processing); });
      }
    }
    auto task = [iAction, this](std::size_t index) mutable {
      m_queues[index].pause();
      iAction(Resumer(&m_queues[index]), index);
    };
    std::size_t index = 0;
    {
      //Need to have sole access to m_sharedTask and m_availableQueues to avoid race conditions
      std::lock_guard<edm::SpinLock> lock{m_sharedTaskAndQueue};

      if UNLIKELY (not m_availableQueues.try_pop(index)) {
        assert(m_sharedTask == nullptr);
        m_sharedTask = new ConcurrentTransitionQueuedTask(iGroup, std::move(task));
        return;
      }
    }
    //This must be out of the critical section to avoid possible lock inversion with the locks in ConcurrentTransitionTaskQueue::pickNextTask
    m_queues[index].push(iGroup, std::move(task));
  }

  template <typename T>
    requires requires(T&& t, ConcurrentTransitionsTaskQueue::Resumer&& r, size_t s) { t(std::move(r), s); }
  void ConcurrentTransitionsTaskQueue::pushToAllAndPause(oneapi::tbb::task_group& iGroup, T&& iAction) {
    if (m_processing) {
      m_processing->store(false);
    }
    size_t index = 0;
    for (auto& q : m_queues) {
      q.push(iGroup, [iAction, this](std::size_t index) mutable {
        m_queues[index].pause();
        iAction(Resumer(&m_queues[index]), index);
      });
      ++index;
    }
  }

}  // namespace edm

#endif
