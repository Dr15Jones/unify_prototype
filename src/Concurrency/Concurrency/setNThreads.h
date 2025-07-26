#ifndef Concurrency_setNThreads_h
#define Concurrency_setNThreads_h
//
//  setNThreads.h
//  CMSSW
//
//  Created by Chris Jones on 7/24/20.
//
#include <memory>
#include "Concurrency/ThreadsController.h"

namespace edm {
  //This guarantees that the previous ThreadsController is destroyed before a new one starts
  // At one time certain TBB control elements required such behavior.
  unsigned int setNThreads(unsigned int iNThreads, unsigned int iStackSize, std::unique_ptr<ThreadsController>& oPtr);
}  // namespace edm
#endif /* Concurrency_setNThreads_h */
