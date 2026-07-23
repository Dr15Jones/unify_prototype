#include <catch2/catch_all.hpp>
#include "ConditionsHandling/ValidityInterval.h"

namespace {
  TEST_CASE("ValidityInterval", "[ValidityInterval]") {
    SECTION("default construction") {
      edm::ValidityInterval interval;
      REQUIRE(interval.begin().size() == 0);
      REQUIRE(interval.end().size() == 0);
    }

    SECTION("construction with begin and end") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval interval(runID1, runID2);
      REQUIRE(interval.begin() == runID1);
      REQUIRE(interval.end() == runID2);
    }

    SECTION("copy construction") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval original(runID1, runID2);
      edm::ValidityInterval copy(original);
      REQUIRE(copy.begin() == runID1);
      REQUIRE(copy.end() == runID2);
      REQUIRE(copy == original);
    }

    SECTION("copy assignment") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval original(runID1, runID2);
      edm::ValidityInterval copy;
      copy = original;
      REQUIRE(copy.begin() == runID1);
      REQUIRE(copy.end() == runID2);
      REQUIRE(copy == original);
    }

    SECTION("move construction") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval original(runID1, runID2);
      edm::ValidityInterval copy(std::move(original));
      REQUIRE(copy.begin() == runID1);
      REQUIRE(copy.end() == runID2);
    }

    SECTION("move assignment") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval original(runID1, runID2);
      edm::ValidityInterval copy;
      copy = std::move(original);
      REQUIRE(copy.begin() == runID1);
      REQUIRE(copy.end() == runID2);
    }

    SECTION("equality") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval interval1(runID1, runID2);
      edm::ValidityInterval interval2(runID1, runID2);
      edm::ValidityInterval interval3(runID1, edm::TransitionRecordID(3U));
      REQUIRE(interval1 == interval2);
      REQUIRE(!(interval1 != interval2));
      REQUIRE(interval1 != interval3);
      REQUIRE(!(interval1 == interval3));
    }

    SECTION("validFor with closed interval") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      auto runID3 = edm::TransitionRecordID(3U);
      auto runID4 = edm::TransitionRecordID(4U);
      edm::ValidityInterval interval(runID1, runID3);
      REQUIRE(interval.validFor(runID1) == true);
      REQUIRE(interval.validFor(runID2) == true);
      REQUIRE(interval.validFor(runID3) == false);
      REQUIRE(interval.validFor(runID4) == false);
    }

    SECTION("validFor with too small interval") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto lumiID1 = edm::TransitionRecordID(runID1, 1U);
      auto lumiID2 = edm::TransitionRecordID(runID1, 2U);
      edm::ValidityInterval interval(runID1, lumiID2);
      REQUIRE(interval.validFor(runID1) == false);
      REQUIRE(interval.validFor(lumiID1) == true);
      REQUIRE(interval.validFor(lumiID2) == false);
    }

    SECTION("validFor with open-ended interval") {
      auto runID1 = edm::TransitionRecordID(1U);
      edm::ValidityInterval interval(runID1, edm::TransitionRecordID());
      REQUIRE(interval.validFor(runID1) == true);
      REQUIRE(interval.validFor(edm::TransitionRecordID(2U)) == true);
      REQUIRE(interval.validFor(edm::TransitionRecordID(100U)) == true);
    }

    SECTION("validFor with empty interval") {
      edm::ValidityInterval interval;
      auto runID = edm::TransitionRecordID(1U);
      REQUIRE(interval.validFor(runID) == false);
    }

    SECTION("validFor with lumi-level IDs") {
      auto runID = edm::TransitionRecordID(1U);
      auto lumiID1 = edm::TransitionRecordID(runID, 1U);
      auto lumiID2 = edm::TransitionRecordID(runID, 2U);
      auto lumiID3 = edm::TransitionRecordID(runID, 3U);
      auto lumiID4 = edm::TransitionRecordID(runID, 4U);
      edm::ValidityInterval interval(lumiID1, lumiID3);
      REQUIRE(interval.validFor(runID) == false);
      REQUIRE(interval.validFor(lumiID1) == true);
      REQUIRE(interval.validFor(lumiID2) == true);
      REQUIRE(interval.validFor(lumiID3) == false);
      REQUIRE(interval.validFor(lumiID4) == false);
    }

    SECTION("operators") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval interval1(runID1, runID2);
      edm::ValidityInterval interval2(runID1, runID2);
      edm::ValidityInterval interval3(runID2, runID1);
      REQUIRE(interval1 == interval2);
      REQUIRE(interval1 != interval3);
    }

    SECTION("overlapWith with non-overlapping intervals") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      auto runID3 = edm::TransitionRecordID(3U);
      auto runID4 = edm::TransitionRecordID(4U);
      edm::ValidityInterval interval1(runID1, runID2);
      edm::ValidityInterval interval2(runID3, runID4);
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin().size() == 0);
      REQUIRE(interval1.end().size() == 0);
    }

    SECTION("overlapWith with overlapping intervals") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      auto runID3 = edm::TransitionRecordID(3U);
      auto runID4 = edm::TransitionRecordID(4U);
      edm::ValidityInterval interval1(runID1, runID4);
      edm::ValidityInterval interval2(runID2, runID3);
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin() == runID2);
      REQUIRE(interval1.end() == runID3);
    }

    SECTION("overlapWith with one empty interval") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval interval1;
      edm::ValidityInterval interval2(runID1, runID2);
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin() == runID1);
      REQUIRE(interval1.end() == runID2);
    }

    SECTION("overlapWith with open-ended intervals") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      auto runID3 = edm::TransitionRecordID(3U);
      edm::ValidityInterval interval1(runID1, edm::TransitionRecordID());
      edm::ValidityInterval interval2(runID2, runID3);
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin() == runID2);
      REQUIRE(interval1.end() == interval1.end());
    }

    SECTION("overlapWith with both open-ended intervals") {
      auto runID1 = edm::TransitionRecordID(1U);
      auto runID2 = edm::TransitionRecordID(2U);
      edm::ValidityInterval interval1(runID1, edm::TransitionRecordID());
      edm::ValidityInterval interval2(runID2, edm::TransitionRecordID());
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin() == runID2);
      REQUIRE(interval1.end().size() == 0);
    }

    SECTION("overlapWith with lumi-level IDs") {
      auto runID = edm::TransitionRecordID(1U);
      auto lumiID1 = edm::TransitionRecordID(runID, 1U);
      auto lumiID2 = edm::TransitionRecordID(runID, 2U);
      auto lumiID3 = edm::TransitionRecordID(runID, 3U);
      auto lumiID4 = edm::TransitionRecordID(runID, 4U);
      edm::ValidityInterval interval1(lumiID1, lumiID4);
      edm::ValidityInterval interval2(lumiID2, lumiID3);
      interval1.overlapWith(interval2);
      REQUIRE(interval1.begin() == lumiID2);
      REQUIRE(interval1.end() == lumiID3);
    }
  }
}