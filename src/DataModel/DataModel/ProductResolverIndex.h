#ifndef DataModel_ProductResolverIndex_h
#define DataModel_ProductResolverIndex_h
#include <limits>

namespace edm {
  class ProductResolverIndex {
  public:
    using Value_t = unsigned int;

    constexpr ProductResolverIndex() noexcept = default;
    constexpr explicit ProductResolverIndex(Value_t iValue) noexcept : index_{iValue} {}
    constexpr ProductResolverIndex(ProductResolverIndex const&) noexcept = default;
    constexpr ProductResolverIndex(ProductResolverIndex&&) noexcept = default;

    constexpr ProductResolverIndex& operator=(ProductResolverIndex const&) noexcept = default;
    constexpr ProductResolverIndex& operator=(ProductResolverIndex&&) noexcept = default;

    constexpr bool operator==(ProductResolverIndex iOther) const noexcept { return iOther.index_ == index_; }
    constexpr bool operator!=(ProductResolverIndex iOther) const noexcept { return iOther.index_ != index_; }

    constexpr Value_t value() const noexcept { return index_; }
    constexpr bool isUninitialized() const noexcept { return index_ == s_uninitializedValue; }


  private:
    static constexpr Value_t s_uninitializedValue = std::numeric_limits<Value_t>::max();
    Value_t index_ = s_uninitializedValue;
  };
}
#endif