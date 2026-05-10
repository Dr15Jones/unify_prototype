#include "TransitionHandling/TransitionRecordID.h"

namespace edm {
  std::strong_ordering TransitionRecordID::operator<=>(const TransitionRecordID& iRHS) const noexcept {
    for (size_t i = 0; i < kMaxWordsInID; ++i) {
      if (auto cmp = id_[i] <=> iRHS.id_[i]; cmp != std::strong_ordering::equal) {
        return cmp;
      }
    }
    if (wordCount() <= kMaxWordsInID && iRHS.wordCount() <= kMaxWordsInID) {
      return std::strong_ordering::equal;
    }
    auto thisExtended = address(extended_);
    auto rhsExtended = address(iRHS.extended_);
    if (thisExtended == nullptr && rhsExtended == nullptr) {
      return std::strong_ordering::equal;
    }
    if (thisExtended == nullptr) {
      return std::strong_ordering::less;
    }
    if (rhsExtended == nullptr) {
      return std::strong_ordering::greater;
    }
    auto thisExtendedCount = wordCount() - kMaxWordsInID;
    auto rhsExtendedCount = iRHS.wordCount() - kMaxWordsInID;
    for (size_t i = 0; (i < thisExtendedCount && i < rhsExtendedCount); ++i) {
      if (auto cmp = thisExtended[i] <=> rhsExtended[i]; cmp != std::strong_ordering::equal) {
        return cmp;
      }
    }
    if (thisExtendedCount < rhsExtendedCount) {
      return std::strong_ordering::less;
    }
    if (thisExtendedCount > rhsExtendedCount) {
      return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
  }

  void TransitionRecordID::addAtEnd(std::uint32_t value) {
    auto currentUsedWords = wordCount();
    if (currentUsedWords < kMaxWordsInID) {
      id_[currentUsedWords] = value;
      setWordCount(currentUsedWords + 1);
    } else {
      //need to move to extended storage
      if (currentUsedWords == kMaxWordsInID) {
        auto newExtended = new std::uint32_t[1];
        assert((reinterpret_cast<std::uintptr_t>(newExtended) & kWordCountMask) == 0);

        newExtended[0] = value;
        extended_ = reinterpret_cast<std::uintptr_t>(newExtended);
        setWordCount(currentUsedWords + 1);
      } else {
        auto addr = address(extended_);
        const auto newWordCount = currentUsedWords + 1;
        assert(newWordCount <= kWordCountMask);  // need to stay within the word count mask limit
        const auto presentExtendedSize = currentUsedWords - kMaxWordsInID;
        auto newAddr = new std::uint32_t[presentExtendedSize + 1];
        assert((reinterpret_cast<std::uintptr_t>(newAddr) & kWordCountMask) == 0);
        std::copy(addr, addr + presentExtendedSize, newAddr);
        newAddr[presentExtendedSize] = value;
        releaseMemory(extended_);
        extended_ = reinterpret_cast<std::uintptr_t>(newAddr);
        setWordCount(newWordCount);
      }
    }
  }

}  // namespace edm