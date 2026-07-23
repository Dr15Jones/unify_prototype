#include <catch2/catch_all.hpp>
#include "DataModel/TransitionRecordID.h"

TEST_CASE("TransitionRecordID can be constructed and compared") {
  SECTION("default constructed") {
    edm::TransitionRecordID id1;
    edm::TransitionRecordID id2;
    REQUIRE(id1 == id2);
  }
  SECTION("different IDs") {
    edm::TransitionRecordID id1(1U);
    edm::TransitionRecordID id2(2U);
    REQUIRE(id1 != id2);
    REQUIRE(id1 < id2);
  }
  SECTION("nested IDs") {
    edm::TransitionRecordID id1(1U);
    edm::TransitionRecordID id1_other(1U);
    edm::TransitionRecordID id1_2(id1, 2U);
    edm::TransitionRecordID id1_2_extra(id1, 2U);
    edm::TransitionRecordID id1_2_other(id1_other, 2U);
    edm::TransitionRecordID id1_3(id1, 3U);
    edm::TransitionRecordID id1_2_4(id1_2, 4U);
    REQUIRE(id1 == id1_other);
    REQUIRE(id1 != id1_2);
    REQUIRE(id1 < id1_2);
    REQUIRE(id1_2 == id1_2_extra);
    REQUIRE(id1_2 == id1_2_other);
    REQUIRE(id1 < id1_3);
    REQUIRE(id1_2 < id1_2_4);
    REQUIRE(id1_3 > id1_2_4);
  }
  SECTION("64 bit IDs") {
    edm::TransitionRecordID id1(0x0000000100000002ULL);
    edm::TransitionRecordID id2(0x0000000100000003ULL);
    edm::TransitionRecordID id1_other(0x0000000100000002ULL);
    REQUIRE(id1 == id1_other);
    REQUIRE(id1 != id2);
    REQUIRE(id1 < id2);
  }
  SECTION("more than 4 words") {
    edm::TransitionRecordID id1(1U);
    REQUIRE(id1.size() == 1);
    edm::TransitionRecordID id2(id1, 2U);
    REQUIRE(id2.size() == 2);
    edm::TransitionRecordID id3(id2, 3U);
    REQUIRE(id3.size() == 3);
    edm::TransitionRecordID id4(id3, 4U);
    REQUIRE(id4.size() == 4);
    edm::TransitionRecordID id5(id4, 5U);
    REQUIRE(id5.size() == 5);
    REQUIRE(id1 < id2);
    REQUIRE(id2 < id3);
    REQUIRE(id3 < id4);
    REQUIRE(id4 < id5);
    REQUIRE(id5 > id1);
    edm::TransitionRecordID id5_other(id4, 5U);
    REQUIRE(id5_other.size() == 5);
    REQUIRE(id5 == id5_other);
    edm::TransitionRecordID id5_copy(id5);
    REQUIRE(id5_copy.size() == 5);
    REQUIRE(id5 == id5_copy);
    edm::TransitionRecordID id5_move(std::move(id5));
    REQUIRE(id5_move.size() == 5);
    REQUIRE(id5_move == id5_copy);
  }
  SECTION("copy and move") {
    edm::TransitionRecordID id1(1U);
    edm::TransitionRecordID id2(id1);
    REQUIRE(id1 == id2);
    edm::TransitionRecordID id3(std::move(id1));
    REQUIRE(id2 == id3);
    edm::TransitionRecordID id4;
    id4 = id2;
    REQUIRE(id4 == id2);
    edm::TransitionRecordID id5;
    id5 = std::move(id4);
    REQUIRE(id5 == id2);
  }
  SECTION("next() method") {
    edm::TransitionRecordID id1(1U);
    auto next1 = id1.next();
    REQUIRE(next1 == edm::TransitionRecordID(2U));
    REQUIRE(id1 == edm::TransitionRecordID(1U));

    edm::TransitionRecordID id2(0xFFFFFFFFULL);
    auto next2 = id2.next();
    REQUIRE(next2 == edm::TransitionRecordID(0x100000000ULL));

    edm::TransitionRecordID id3(1U);
    edm::TransitionRecordID id3_child(id3, 2U);
    auto next3 = id3_child.next();
    REQUIRE(next3 == edm::TransitionRecordID(id3, 3U));
    REQUIRE(id3_child == edm::TransitionRecordID(id3, 2U));

    edm::TransitionRecordID id4(0xFFFFFFFFU);
    edm::TransitionRecordID id4_child(id4, 0xFFFFFFFFULL);
    auto next4 = id4_child.next();
    REQUIRE(next4 == edm::TransitionRecordID(id4, 0x100000000ULL));

    edm::TransitionRecordID id5;
    auto next5 = id5.next();
    REQUIRE(next5 == edm::TransitionRecordID());

    edm::TransitionRecordID id6(1U);
    auto next6a = id6.next();
    auto next6b = id6.next();
    REQUIRE(next6a == next6b);
    REQUIRE(next6a == edm::TransitionRecordID(2U));
  }
}
