#ifndef DataModel_ProductTransitionRecordIndex_h
#define DataModel_ProductTransitionRecordIndex_h
#include <limits>

namespace edm {
  class ProductTransitionRecordIndex {
  public:
    using Value_t = unsigned int;

    constexpr ProductTransitionRecordIndex() noexcept = default;
    constexpr explicit ProductTransitionRecordIndex(Value_t iValue) noexcept : index_{iValue} {}
    constexpr ProductTransitionRecordIndex(ProductTransitionRecordIndex const&) noexcept = default;
    constexpr ProductTransitionRecordIndex(ProductTransitionRecordIndex&&) noexcept = default;

    constexpr ProductTransitionRecordIndex& operator=(ProductTransitionRecordIndex const&) noexcept = default;
    constexpr ProductTransitionRecordIndex& operator=(ProductTransitionRecordIndex&&) noexcept = default;

    constexpr bool operator==(ProductTransitionRecordIndex iOther) const noexcept { return iOther.index_ == index_; }
    constexpr bool operator!=(ProductTransitionRecordIndex iOther) const noexcept { return iOther.index_ != index_; }

    constexpr Value_t value() const noexcept { return index_; }
    constexpr bool isUninitialized() const noexcept { return index_ == s_uninitializedValue; }


  private:
    static constexpr Value_t s_uninitializedValue = std::numeric_limits<Value_t>::max();
    Value_t index_ = s_uninitializedValue;
  };
}
#endif