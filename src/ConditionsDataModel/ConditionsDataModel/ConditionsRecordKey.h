#ifndef ConditionsDataModel_ConditionsRecordKey_h
#define ConditionsDataModel_ConditionsRecordKey_h

#include "Base/TypeIDBase.h"
#include <string_view>

namespace edm {
  class ConditionsRecordKey {
  public:
    constexpr ConditionsRecordKey() = default;
    constexpr ConditionsRecordKey(TypeIDBase typeID) : typeID_(typeID) {}

    constexpr TypeIDBase const& typeID() const { return typeID_; }
    std::string name() const;

    constexpr std::strong_ordering operator<=>(ConditionsRecordKey const& other) const noexcept = default;

    template <typename T>
    static constexpr ConditionsRecordKey makeKey() {
      return ConditionsRecordKey(TypeIDBase(typeid(T)));
    }

  private:
    TypeIDBase typeID_;
  };
}  // namespace edm
#endif
