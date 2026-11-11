#ifndef TransitionHandling_ConcurrentTransitionTaskQueue_h
#define TransitionHandling_ConcurrentTransitionTaskQueue_h
// -*- C++ -*-
//
// Package:     TransitionHandling
// Class  :     ConcurrentTransitionTaskQueue
//
/**\class ConcurrentTransitionTaskQueue ConcurrentTransitionTaskQueue.h "TransitionHandling/ConcurrentTransitionTaskQueue.h"

 Description: An implementation detail of ConcurrentTransitionsTaskQueue that manages a single queue of tasks.

 Usage:
 This class manages a single queue of tasks. It is used by ConcurrentTransitionsTaskQueue to manage multiple queues of tasks.
 The class pulls tasks from the queue held by the ConcurrentTransitionsTaskQueue and runs them. It will only run a task if the
 queue is not paused. If the queue is paused, it will wait until the queue is resumed before running the next task.
*/

// system include files
#include <atomic>
#include <cassert>

#include "oneapi/tbb/task_group.h"
#include "oneapi/tbb/concurrent_queue.h"
#include "Utilities/thread_safety_macros.h"
#include "Concurrency/SpinLock.h"

// user include files
#include "TransitionHandling/ConcurrentTransitionTaskBase.h"

// forward declarations
namespace edm {
  class ConcurrentTransitionsTaskQueue;

  class ConcurrentTransitionTaskQueue {
  public:
    explicit ConcurrentTransitionTaskQueue(ConcurrentTransitionsTaskQueue& iSharedQueue,
                                           std::size_t iIndex,
                                           std::shared_ptr<std::atomic<bool>> iProcessing)
        : m_sharedQueue{&iSharedQueue},
          m_index{iIndex},
          m_taskChosen{false},
          m_pickingNextTask{},
          m_processing{iProcessing} {}

    ConcurrentTransitionTaskQueue(ConcurrentTransitionTaskQueue&& iOther)
        : m_tasks(std::move(iOther.m_tasks)),
          m_sharedQueue(iOther.m_sharedQueue),
          m_pauseCount(iOther.m_pauseCount.exchange(0)),
          m_index{iOther.m_index},
          m_taskChosen(iOther.m_taskChosen.exchange(false)),
          m_pickingNextTask{} {
      assert(m_tasks.empty() and m_taskChosen == false and iOther.m_pickingNextTask.isLocked() == false);
    }
    ConcurrentTransitionTaskQueue(const ConcurrentTransitionTaskQueue&) = delete;
    const ConcurrentTransitionTaskQueue& operator=(const ConcurrentTransitionTaskQueue&) = delete;

    ~ConcurrentTransitionTaskQueue();

    // ---------- const member functions ---------------------
    /// Checks to see if the queue has been paused.
    /**\return true if the queue is paused
       * \sa pause(), resume()
       */
    bool isPaused() const { return m_pauseCount.load() != 0; }

    // ---------- member functions ---------------------------
    /// Pauses processing of additional tasks from the queue.
    /**
       * Any task already running will not be paused however once that
       * running task finishes no further tasks will be started.
       * Multiple calls to pause() are allowed, however each call to 
       * pause() must be balanced by a call to resume().
       * \return false if queue was already paused.
       * \sa resume(), isPaused()
       */
    bool pause() { return 1 == ++m_pauseCount; }

    /// Resumes processing if the queue was paused.
    /**
       * Multiple calls to resume() are allowed if there
       * were multiple calls to pause(). Only when we reach as
       * many resume() calls as pause() calls will the queue restart.
       * \return true if the call really restarts the queue
       * \sa pause(), isPaused()
       */
    bool resume();

    /// asynchronously pushes functor iAction into queue
    /**
       * The function will return immediately and iAction will either
       * process concurrently with the calling thread or wait until the
       * protected resource becomes available or until a CPU becomes available.
       * \param[in] iAction Must be a functor that takes no arguments and return no values.
       */
    template <typename T>
      requires requires(T&& t) { t(size_t(0)); }
    void push(oneapi::tbb::task_group&, const T& iAction);

    void processing(std::shared_ptr<std::atomic<bool>> iProcessing) { m_processing = iProcessing; }

  private:
    friend class ConcurrentTransitionTaskBase;

    void pushTask(ConcurrentTransitionTaskBase*);
    ConcurrentTransitionTaskBase* pushAndGetNextTask(ConcurrentTransitionTaskBase*);
    ConcurrentTransitionTaskBase* finishedTask();
    //returns nullptr if a task is already being processed
    ConcurrentTransitionTaskBase* pickNextTask();

    void spawn(ConcurrentTransitionTaskBase&);

    bool tryFromSharedQueue(ConcurrentTransitionTaskBase*& oTask);

    // ---------- member data --------------------------------
    oneapi::tbb::concurrent_queue<ConcurrentTransitionTaskBase*> m_tasks;
    std::shared_ptr<std::atomic<bool>> m_processing;
    ConcurrentTransitionsTaskQueue* m_sharedQueue = nullptr;
    std::atomic<unsigned long> m_pauseCount;
    std::size_t m_index{0};
    std::atomic<bool> m_taskChosen;
    edm::SpinLock m_pickingNextTask;
  };

  template <typename T>
    requires requires(T&& t) { t(size_t(0)); }
  void ConcurrentTransitionTaskQueue::push(oneapi::tbb::task_group& iGroup, const T& iAction) {
    auto* pTask{new ConcurrentTransitionQueuedTask<T>{iGroup, iAction}};
    pushTask(pTask);
  }

}  // namespace edm

#endif
