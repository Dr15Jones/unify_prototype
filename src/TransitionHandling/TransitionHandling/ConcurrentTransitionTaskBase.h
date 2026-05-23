#ifndef TransitionHandling_ConcurrentTransitionTaskBase_h
#define TransitionHandling_ConcurrentTransitionTaskBase_h
// -*- C++ -*-
// Package:     TransitionHandling
// Class  :     ConcurrentTransitionTaskBase
/// Description: Base class for tasks that run asynchronously as part of processing a concurrent transition. This provides access to the transition scheduler and the concurrent transition ID for the stream on which the task is running.
//
// Original Author:  Chris Jones
//         Created:  Thu Feb 21 11:14:39 CST 2013

#include <oneapi/tbb/task_group.h>

namespace edm {
  /** Base class for all tasks held by the ConcurrentTransitionTaskQueue */
  class ConcurrentTransitionTaskBase {
    friend class ConcurrentTransitionTaskQueue;

    oneapi::tbb::task_group* group() { return m_group; }
    virtual void execute(std::size_t index) = 0;

  public:
    virtual ~ConcurrentTransitionTaskBase() = default;

  protected:
    explicit ConcurrentTransitionTaskBase(oneapi::tbb::task_group* iGroup) : m_group(iGroup) {}

  private:
    oneapi::tbb::task_group* m_group;
  };

  template <typename T>
    requires requires(T&& t) { t(std::size_t(0)); }
  class ConcurrentTransitionQueuedTask : public ConcurrentTransitionTaskBase {
  public:
    ConcurrentTransitionQueuedTask(oneapi::tbb::task_group& iGroup, const T& iAction)
        : ConcurrentTransitionTaskBase(&iGroup), m_action(iAction) {}

  private:
    void execute(std::size_t index) final;

    T m_action;
  };

  template <typename T>
    requires requires(T&& t) { t(std::size_t(0)); }
  void ConcurrentTransitionQueuedTask<T>::execute(std::size_t index) {
    // Exception has to swallowed in order to avoid throwing from execute(). The user of ConcurrentTransitionTaskQueue should handle exceptions within m_action().
    CMS_SA_ALLOW try { this->m_action(index); } catch (...) {
    }
  }

}  // namespace edm
#endif