#ifndef DataModel_ProductGetTokenIndex_h
#define DataModel_ProductGetTokenIndex_h
#include <limits>

namespace edm {
  class ProductGetTokenIndex {
  public:
    using Value_t = unsigned int;

    constexpr ProductGetTokenIndex() noexcept = default;
    constexpr explicit ProductGetTokenIndex(Value_t iValue) noexcept : index_{iValue} {}
    constexpr ProductGetTokenIndex(ProductGetTokenIndex const&) noexcept = default;
    constexpr ProductGetTokenIndex(ProductGetTokenIndex&&) noexcept = default;

    constexpr ProductGetTokenIndex& operator=(ProductGetTokenIndex const&) noexcept = default;
    constexpr ProductGetTokenIndex& operator=(ProductGetTokenIndex&&) noexcept = default;

    constexpr bool operator==(ProductGetTokenIndex iOther) const noexcept { return iOther.index_ == index_; }
    constexpr bool operator!=(ProductGetTokenIndex iOther) const noexcept { return iOther.index_ != index_; }

    constexpr Value_t value() const noexcept { return index_; }
    constexpr bool isUninitialized() const noexcept { return index_ == s_uninitializedValue; }


  private:
    static constexpr Value_t s_uninitializedValue = std::numeric_limits<Value_t>::max();
    Value_t index_ = s_uninitializedValue;
  };
}
#endif