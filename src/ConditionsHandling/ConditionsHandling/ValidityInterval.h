#ifndef ConditionsHandling_ValidityInterval_h
#define ConditionsHandling_ValidityInterval_h

#include "DataModel/TransitionRecordID.h"
#include <iostream>

namespace edm {
  class ValidityInterval {
  public:
    ValidityInterval() = default;
    ValidityInterval(TransitionRecordID const& begin, TransitionRecordID const& end) : begin_(begin), end_(end) {}

    ValidityInterval(ValidityInterval const& other) = default;
    ValidityInterval& operator=(ValidityInterval const& other) = default;
    ValidityInterval(ValidityInterval&& other) = default;
    ValidityInterval& operator=(ValidityInterval&& other) = default;

    bool validFor(TransitionRecordID const& recordID) const {
      if (begin_.size() == 0) {
        //special case for empty begin intervals
        return false;
      }
      if (end_.size() == 0) {
        //special case for open-ended end intervals
        return recordID >= begin_;
      }
      return recordID >= begin_ && recordID < end_ and recordID.next() <= end_;
    }

    TransitionRecordID const& begin() const { return begin_; }
    TransitionRecordID const& end() const { return end_; }

    bool operator==(const ValidityInterval& other) const { return other.begin_ == begin_ && other.end_ == end_; }
    bool operator!=(const ValidityInterval& other) const { return !(*this == other); }

    constexpr void overlapWith(ValidityInterval const& other) {
      if (other.begin_.size() == 0) {
        //other is an empty interval, nothing to do
        return;
      }
      if (begin_.size() == 0) {
        //If this is an empty interval, just copy the other
        *this = other;
        return;
      }

      begin_ = std::max(begin_, other.begin_);
      //open intervals should remain open, so if either is open, the union should be open
      if (end_.size() == 0) {
        return;
      }
      if (other.end_.size() == 0) {
        end_ = other.end_;
        return;
      }
      end_ = std::min(end_, other.end_);
      if (end_ < begin_) {
        //if the end is less than the begin, then this is an empty interval
        begin_ = TransitionRecordID{};
        end_ = TransitionRecordID{};
      }
    }

  private:
    TransitionRecordID begin_;
    TransitionRecordID end_;
  };

  inline std::ostream& operator<<(std::ostream& os, ValidityInterval const& interval) {
    os << "ValidityInterval [ " << interval.begin() << ", end=" << interval.end() << " ]";
    return os;
  }
}  // namespace edm
#endif  // ConditionsHandling_ValidityInterval_h