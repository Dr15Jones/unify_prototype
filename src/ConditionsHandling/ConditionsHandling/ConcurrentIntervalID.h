#ifndef ConditionsHandling_ConcurrentIntervalID_h
#define ConditionsHandling_ConcurrentIntervalID_h
/*
Class to represent the ID of a concurrent conditions record. This is used to track which concurrent transition is being processed on each stream and to associate resources with a particular concurrent transition.
*/
#include <cstddef>
#include <iostream>
namespace edm {
  class ConcurrentIntervalID {
  public:
    explicit ConcurrentIntervalID(std::size_t id) : id_(id) {}
    std::size_t id() const { return id_; }

    auto operator<=>(const ConcurrentIntervalID& other) const = default;

  private:
    std::size_t id_;
  };
  inline std::ostream& operator<<(std::ostream& os, ConcurrentIntervalID const& id) {
    return os << "ConcurrentIntervalID(" << id.id() << ")";
  }
}  // namespace edm
#endif
