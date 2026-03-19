#ifndef DataModel_TransitionRecordProductIndex_h
#define DataModel_TransitionRecordProductIndex_h
#include <limits>

namespace edm {
  class TransitionRecordProductIndex {
  public:
    using Value_t = unsigned int;

    constexpr TransitionRecordProductIndex() noexcept = default;
    constexpr explicit TransitionRecordProductIndex(Value_t iValue) noexcept : index_{iValue} {}
    constexpr TransitionRecordProductIndex(TransitionRecordProductIndex const&) noexcept = default;
    constexpr TransitionRecordProductIndex(TransitionRecordProductIndex&&) noexcept = default;

    constexpr TransitionRecordProductIndex& operator=(TransitionRecordProductIndex const&) noexcept = default;
    constexpr TransitionRecordProductIndex& operator=(TransitionRecordProductIndex&&) noexcept = default;

    constexpr bool operator==(TransitionRecordProductIndex iOther) const noexcept { return iOther.index_ == index_; }
    constexpr bool operator!=(TransitionRecordProductIndex iOther) const noexcept { return iOther.index_ != index_; }

    constexpr Value_t value() const noexcept { return index_; }
    constexpr bool isUninitialized() const noexcept { return index_ == s_uninitializedValue; }


  private:
    static constexpr Value_t s_uninitializedValue = std::numeric_limits<Value_t>::max();
    Value_t index_ = s_uninitializedValue;
  };
}
#endif