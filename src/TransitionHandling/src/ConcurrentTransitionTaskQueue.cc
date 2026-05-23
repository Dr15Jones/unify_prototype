// -*- C++ -*-
//
// Package:     Concurrency
// Class  :     ConcurrentTransitionTaskQueue
//
// Implementation:
//     [Notes on implementation]
//
// Original Author:  Chris Jones
//         Created:  Thu Feb 21 11:31:52 CST 2013
// $Id$
//

// system include files
#include "oneapi/tbb/task_group.h"

// user include files
#include "TransitionHandling/ConcurrentTransitionTaskQueue.h"
#include "TransitionHandling/ConcurrentTransitionsTaskQueue.h"

#include "Utilities/Likely.h"
#include "Utilities/make_sentry.h"

using namespace edm;

//
// member functions
//
ConcurrentTransitionTaskQueue::~ConcurrentTransitionTaskQueue() {
  //be certain all tasks have completed
  bool isEmpty = m_tasks.empty();
  bool isTaskChosen = m_taskChosen;
  if ((not isEmpty and not isPaused()) or isTaskChosen) {
    oneapi::tbb::task_group g;
    oneapi::tbb::task_handle last{g.defer([]() {})};
    push(g, [&g, &last](std::size_t) { g.run(std::move(last)); });
    g.wait();
  }
}

void ConcurrentTransitionTaskQueue::spawn(ConcurrentTransitionTaskBase& iTask) {
  auto pTask = &iTask;
  iTask.group()->run([pTask, this]() {
    ConcurrentTransitionTaskBase* t = pTask;
    auto g = pTask->group();
    do {
      t->execute(m_index);
      delete t;
      t = finishedTask();
      if (t and t->group() != g) {
        spawn(*t);
        t = nullptr;
      }
    } while (t != nullptr);
  });
}

bool ConcurrentTransitionTaskQueue::resume() {
  if (0 == --m_pauseCount) {
    auto t = pickNextTask();
    if (nullptr != t) {
      spawn(*t);
    }
    return true;
  }
  return false;
}

void ConcurrentTransitionTaskQueue::pushTask(ConcurrentTransitionTaskBase* iTask) {
  auto t = pushAndGetNextTask(iTask);
  if (nullptr != t) {
    spawn(*t);
  }
}

ConcurrentTransitionTaskBase* ConcurrentTransitionTaskQueue::pushAndGetNextTask(ConcurrentTransitionTaskBase* iTask) {
  ConcurrentTransitionTaskBase* returnValue{nullptr};
  if LIKELY (nullptr != iTask) {
    m_tasks.push(iTask);
    returnValue = pickNextTask();
  }
  return returnValue;
}

ConcurrentTransitionTaskBase* ConcurrentTransitionTaskQueue::finishedTask() {
  m_taskChosen.store(false);
  return pickNextTask();
}

ConcurrentTransitionTaskBase* ConcurrentTransitionTaskQueue::pickNextTask() {
  if UNLIKELY (0 != m_pauseCount)
    return nullptr;
  //need pop task and setting m_taskChosen to be atomic to avoid
  // case where thread pauses just after try_pop failed but then
  // a task is added and that call fails the check on m_taskChosen
  while (m_pickingNextTask.exchange(true)) {
  }
  auto sentry = edm::make_sentry(&m_pickingNextTask, [](auto* v) { v->store(false); });

  if LIKELY (not m_taskChosen.exchange(true)) {
    ConcurrentTransitionTaskBase* t = nullptr;
    if LIKELY (m_tasks.try_pop(t)) {
      return t;
    }
    if LIKELY (tryFromSharedQueue(t)) {
      return t;
    }
    //no task was actually pulled
    m_taskChosen.store(false);
  }
  return nullptr;
}

bool ConcurrentTransitionTaskQueue::tryFromSharedQueue(ConcurrentTransitionTaskBase*& oTask) {
  return m_sharedQueue->tryFromSharedQueue(oTask, m_index, *m_processing);
}

//
// const member functions
//

//
// static member functions
//
