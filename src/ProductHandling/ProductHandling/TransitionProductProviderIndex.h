#ifndef DataModel_TransitionProductProviderIndex_h
#define DataModel_TransitionProductProviderIndex_h
#include <limits>

namespace edm {
  class TransitionProductProviderIndex {
  public:
    using Value_t = unsigned int;

    constexpr TransitionProductProviderIndex() noexcept = default;
    constexpr explicit TransitionProductProviderIndex(Value_t iValue) noexcept : index_{iValue} {}
    constexpr TransitionProductProviderIndex(TransitionProductProviderIndex const&) noexcept = default;
    constexpr TransitionProductProviderIndex(TransitionProductProviderIndex&&) noexcept = default;

    constexpr TransitionProductProviderIndex& operator=(TransitionProductProviderIndex const&) noexcept = default;
    constexpr TransitionProductProviderIndex& operator=(TransitionProductProviderIndex&&) noexcept = default;

    constexpr bool operator==(TransitionProductProviderIndex iOther) const noexcept { return iOther.index_ == index_; }
    constexpr bool operator!=(TransitionProductProviderIndex iOther) const noexcept { return iOther.index_ != index_; }

    constexpr Value_t value() const noexcept { return index_; }
    constexpr bool isUninitialized() const noexcept { return index_ == s_uninitializedValue; }

  private:
    static constexpr Value_t s_uninitializedValue = std::numeric_limits<Value_t>::max();
    Value_t index_ = s_uninitializedValue;
  };
}  // namespace edm
#endif