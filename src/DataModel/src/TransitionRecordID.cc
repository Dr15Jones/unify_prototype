#include "DataModel/TransitionRecordID.h"

namespace edm {
  std::strong_ordering TransitionRecordID::operator<=>(const TransitionRecordID& iRHS) const noexcept {
    if (this == &iRHS) {
      return std::strong_ordering::equal;
    }
    std::uint32_t const* id = this->begin();
    std::uint32_t const* rhsID = iRHS.begin();
    std::uint32_t const* idEnd = this->end();
    std::uint32_t const* rhsIDEnd = iRHS.end();
    for (; id != idEnd && rhsID != rhsIDEnd; ++id, ++rhsID) {
      if (auto cmp = *id <=> *rhsID; cmp != std::strong_ordering::equal) {
        return cmp;
      }
    }
    if (id == idEnd && rhsID == rhsIDEnd) {
      return std::strong_ordering::equal;
    }
    if (id == idEnd) {
      return std::strong_ordering::less;
    }
    return std::strong_ordering::greater;
  }

  void TransitionRecordID::addAtEnd(std::uint32_t value) {
    const auto currentUsedWords = size();
    if (currentUsedWords < kArraySize) {
      id_.array_[currentUsedWords] = value;
      size_ = currentUsedWords + 1;
    } else if (currentUsedWords == kArraySize) {
      std::uint32_t* newContainer = new std::uint32_t[kArraySize + 1];
      std::copy(id_.array_.begin(), id_.array_.end(), newContainer);
      newContainer[kArraySize] = value;
      id_.pointers_.begin_ = newContainer;
      id_.pointers_.end_ = newContainer + kArraySize + 1;
      size_ = currentUsedWords + 1;
    } else {
      std::uint32_t* newContainer = new std::uint32_t[currentUsedWords + 1];
      std::copy(id_.pointers_.begin_, id_.pointers_.end_, newContainer);
      newContainer[currentUsedWords] = value;
      delete[] id_.pointers_.begin_;
      id_.pointers_.begin_ = newContainer;
      id_.pointers_.end_ = newContainer + currentUsedWords + 1;
      size_ = currentUsedWords + 1;
    }
  }

  std::ostream& operator<<(std::ostream& os, TransitionRecordID const& id) {
    os << "TransitionRecordID(";
    for (auto it = id.begin(); it != id.end(); ++it) {
      if (it != id.begin()) {
        os << ", ";
      }
      os << *it;
    }
    os << ")";
    return os;
  }
}  // namespace edm