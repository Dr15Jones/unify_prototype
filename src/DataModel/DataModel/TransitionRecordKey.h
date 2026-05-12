#ifndef DataModel_TransitionRecordKey_h
#define DataModel_TransitionRecordKey_h

#include "Base/TypeIDBase.h"
#include <string_view>

namespace edm {
  class TransitionRecordKey {
  public:
    constexpr TransitionRecordKey() = default;
    constexpr TransitionRecordKey(TypeIDBase typeID) : typeID_(typeID) {}

    constexpr TypeIDBase const& typeID() const { return typeID_; }
    std::string name() const;

    constexpr std::strong_ordering operator<=>(TransitionRecordKey const& other) const noexcept = default;

    template <typename T>
    static constexpr TransitionRecordKey makeKey() {
      return TransitionRecordKey(TypeIDBase(typeid(T)));
    }

  private:
    TypeIDBase typeID_;
  };
}  // namespace edm
#endif