#ifndef DataProductBase_Wrapper_h
#define DataProductBase_Wrapper_h

#include "DataProductBase/WrapperBase.h"
#include <optional>
namespace edm {
  template <typename T>
  class Wrapper : public WrapperBase {
  public:
    Wrapper() = default;
    explicit Wrapper(T const& iData) : data_(iData) {}
    explicit Wrapper(T&& iData) : data_(std::move(iData)) {}
    ~Wrapper() override = default;

    T const& product() const { return data_.value(); }
  private:
    std::optional<T> data_;
  };
}  // namespace edm

#endif