#ifndef TransitionHandling_TransitionRecordID_h
#define TransitionHandling_TransitionRecordID_h
#include <cstdint>
#include <array>
#include <algorithm>
#include <cassert>

namespace edm {
  class TransitionRecordID {
  public:
    TransitionRecordID() : id_{}, extended_(0) {}
    constexpr explicit TransitionRecordID(std::uint32_t id) noexcept : id_{}, extended_(0) { addAtEnd(id); }
    constexpr explicit TransitionRecordID(std::uint64_t id) noexcept : id_{}, extended_(0) {
      //high word first
      addAtEnd(static_cast<std::uint32_t>((id >> 32) & 0xFFFFFFFF));
      addAtEnd(static_cast<std::uint32_t>(id & 0xFFFFFFFF));
    }
    explicit TransitionRecordID(TransitionRecordID const& iOther, std::uint32_t id) : TransitionRecordID(iOther) {
      addAtEnd(id);
    }
    explicit TransitionRecordID(TransitionRecordID const& iOther, std::uint64_t id) : TransitionRecordID(iOther) {
      addAtEnd(static_cast<std::uint32_t>((id >> 32) & 0xFFFFFFFF));
      addAtEnd(static_cast<std::uint32_t>(id & 0xFFFFFFFF));
    }
    TransitionRecordID(const TransitionRecordID& iOther) : id_(iOther.id_), extended_(copyExtended(iOther.extended_)) {}

    ~TransitionRecordID() noexcept { releaseMemory(extended_); }
    TransitionRecordID(TransitionRecordID&& iOther) noexcept : id_(iOther.id_), extended_(iOther.extended_) {
      iOther.extended_ = 0;
    }
    TransitionRecordID& operator=(TransitionRecordID&& iOther) noexcept {
      if (this != &iOther) {
        releaseMemory(extended_);
        id_ = iOther.id_;
        extended_ = iOther.extended_;
        iOther.extended_ = 0;
      }
      return *this;
    }

    TransitionRecordID& operator=(const TransitionRecordID& iOther) {
      if (this != &iOther) {
        releaseMemory(extended_);
        id_ = iOther.id_;
        extended_ = copyExtended(iOther.extended_);
      }
      return *this;
    }

    std::strong_ordering operator<=>(const TransitionRecordID& iRHS) const noexcept;
    constexpr bool operator==(const TransitionRecordID& iRHS) const noexcept {
      return (*this <=> iRHS) == std::strong_ordering::equal;
    }

    //includes both the fixed and extended storage word counts
    constexpr unsigned char wordCount() const noexcept { return extended_ & kWordCountMask; }

  private:
    void addAtEnd(std::uint32_t value);
    void releaseMemory(std::uintptr_t ptr) noexcept {
      auto addr = address(ptr);
      if (addr) {
        delete[] addr;
      }
    }
    constexpr void setWordCount(unsigned char count) noexcept {
      extended_ = (extended_ & kAddressMask) | (count & kWordCountMask);
    }
    static std::uint32_t* address(std::uintptr_t iValue) noexcept {
      return reinterpret_cast<std::uint32_t*>(iValue & kAddressMask);
    }
    static std::uintptr_t copyExtended(std::uintptr_t iValue) {
      std::uint32_t const* const addr = address(iValue);
      if (addr) {
        const auto wordCount = iValue & kWordCountMask;
        const auto copyCount = wordCount - kMaxWordsInID;
        auto newAddr = new std::uint32_t[copyCount];
        assert((reinterpret_cast<std::uintptr_t>(newAddr) & kWordCountMask) == 0);
        std::copy(addr, addr + copyCount, newAddr);
        return reinterpret_cast<std::uintptr_t>(newAddr) | wordCount;
      }
      return iValue;
    }
    //linux and macos guarantee new[] returns memory aligned enough to hold any type, so we can steal the low bits to store the word count
    static constexpr const std::uintptr_t kWordCountMask = 0xF;
    static constexpr const std::uintptr_t kAddressMask = ~kWordCountMask;
    static constexpr const unsigned char kArraySize = 4;
    static constexpr const unsigned char kMaxWordsInID = kArraySize;
    std::array<std::uint32_t, kArraySize> id_;
    std::uintptr_t extended_ = 0;
  };

}  // namespace edm

#endif