#ifndef TransitionHandling_ConcurrentTransitionID_h
#define TransitionHandling_ConcurrentTransitionID_h
/*
Class to represent the ID of a concurrent transition. This is used to track which concurrent transition is being processed on each stream and to associate resources with a particular concurrent transition.
*/
#include <cstddef>
#include <iostream>
namespace edm {
  class ConcurrentTransitionID {
  public:
    explicit ConcurrentTransitionID(std::size_t id) : id_(id) {}
    std::size_t id() const { return id_; }

    auto operator<=>(const ConcurrentTransitionID& other) const = default;

  private:
    std::size_t id_;
  };
  inline std::ostream& operator<<(std::ostream& os, ConcurrentTransitionID const& id) {
    return os << "ConcurrentTransitionID(" << id.id() << ")";
  }
}  // namespace edm
#endif
