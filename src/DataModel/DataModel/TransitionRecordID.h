#ifndef TransitionHandling_TransitionRecordID_h
#define TransitionHandling_TransitionRecordID_h
#include <cstdint>
#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>

/*
 * Transition records are grouped into a hierarchy of transitions. For example, a Run transition may have multiple Lumi transitions
 * and each Lumi transition may have multiple Event transitions. Each transition record is assigned a TransitionRecordID which encodes
 *  the position of the transition in the hierarchy. The TransitionRecordID is designed to be compact for the common case of a small 
 * number of transitions in the hierarchy, but can also handle an arbitrary number of transitions if needed.
 * 
 * The hierarchy is encoded from the highest level to the lowest level. Therefore when doing comparisons, the highest level (i.e. the 
 * value in the front of the container) is compared first.
 */

namespace edm {
  class TransitionRecordID {
  public:
    constexpr TransitionRecordID() noexcept : size_(0) { id_.array_ = {0}; }
    constexpr explicit TransitionRecordID(std::uint32_t id) noexcept : id_{}, size_(0) { addAtEnd(id); }
    constexpr explicit TransitionRecordID(std::uint64_t id) noexcept : id_{}, size_(0) {
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
    TransitionRecordID(const TransitionRecordID& iOther) : size_(iOther.size_) { copyID(iOther.id_); }

    ~TransitionRecordID() noexcept { releaseMemory(); }
    TransitionRecordID(TransitionRecordID&& iOther) noexcept : size_(iOther.size_) {
      iOther.size_ = 0;
      moveID(iOther.id_);
    }
    TransitionRecordID& operator=(TransitionRecordID&& iOther) noexcept {
      if (this != &iOther) {
        releaseMemory();
        size_ = iOther.size_;
        moveID(iOther.id_);
      }
      return *this;
    }

    TransitionRecordID& operator=(const TransitionRecordID& iOther) {
      if (this != &iOther) {
        releaseMemory();
        size_ = iOther.size_;
        copyID(iOther.id_);
      }
      return *this;
    }

    TransitionRecordID next() const {
      TransitionRecordID nextID(*this);
      if (nextID.size() != 0) {
        if (0 == ++(*(nextID.end() - 1)) and nextID.size() > 1) {
          ++(*(nextID.end() - 2));  //increment the next to last element if the last element overflows
        }
      }
      return nextID;
    }

    std::strong_ordering operator<=>(const TransitionRecordID& iRHS) const noexcept;
    constexpr bool operator==(const TransitionRecordID& iRHS) const noexcept {
      return (*this <=> iRHS) == std::strong_ordering::equal;
    }

    //includes both the fixed and extended storage word counts
    constexpr unsigned char size() const noexcept { return size_; }

    uint32_t* begin() noexcept {
      if (size_ <= kArraySize) {
        return id_.array_.begin();
      }
      return id_.pointers_.begin_;
    }
    uint32_t* end() noexcept {
      if (size_ <= kArraySize) {
        return id_.array_.begin() + size_;
      }
      return id_.pointers_.end_;
    }
    uint32_t const* begin() const noexcept {
      if (size_ <= kArraySize) {
        return id_.array_.begin();
      }
      return id_.pointers_.begin_;
    }
    uint32_t const* end() const noexcept {
      if (size_ <= kArraySize) {
        return id_.array_.begin() + size_;
      }
      return id_.pointers_.end_;
    }

  private:
    static constexpr const unsigned char kArraySize = 4;

    union ID {
      std::array<std::uint32_t, kArraySize> array_;
      struct {
        std::uint32_t* begin_;
        std::uint32_t* end_;
      } pointers_;
    };

    void addAtEnd(std::uint32_t value);
    void releaseMemory() noexcept {
      if (size_ > kArraySize) {
        delete[] id_.pointers_.begin_;
      }
    }
    void moveID(ID& iOther) {
      if (size_ <= kArraySize) {
        id_.array_ = iOther.array_;
      } else {
        id_.pointers_.begin_ = iOther.pointers_.begin_;
        id_.pointers_.end_ = iOther.pointers_.end_;
        iOther.pointers_.begin_ = nullptr;
        iOther.pointers_.end_ = nullptr;
      }
    }
    void copyID(ID const& iValue) {
      if (size_ <= kArraySize) {
        id_.array_ = iValue.array_;
      } else {
        id_.pointers_.begin_ = new std::uint32_t[size_];
        id_.pointers_.end_ = id_.pointers_.begin_ + size_;
        std::copy(iValue.pointers_.begin_, iValue.pointers_.end_, id_.pointers_.begin_);
      }
    }
    ID id_;
    std::uint32_t size_ = 0;
  };

  std::ostream& operator<<(std::ostream& os, TransitionRecordID const& id);
}  // namespace edm

#endif