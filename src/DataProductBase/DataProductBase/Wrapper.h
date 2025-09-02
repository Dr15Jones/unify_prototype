#ifndef DataProductBase_Wrapper_h
#define DataProductBase_Wrapper_h

#include "DataProductBase/WrapperBase.h"
namespace edm {
  template <typename T>
  class Wrapper : public WrapperBase {};
}  // namespace edm

#endif