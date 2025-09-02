#include <catch2/catch.hpp>
#include "DataModel/TransitionRecordKey.h"

namespace trktest {
  struct DummyRecord {};
}  // namespace trktest
TEST_CASE("TransitionRecordKey", "[TransitionRecordKey]") {
  SECTION("Default Constructor") {
    edm::TransitionRecordKey key;
    REQUIRE(key.typeID() == edm::TypeIDBase());
  }

  SECTION("Constructor with TypeIDBase") {
    edm::TypeIDBase typeID(typeid(int));
    edm::TransitionRecordKey key(typeID);
    REQUIRE(key.typeID() == typeID);
  }

  SECTION("Name Method") {
    edm::TypeIDBase typeID(typeid(double));
    edm::TransitionRecordKey key(typeID);
    REQUIRE(key.name() == "double");

    edm::TypeIDBase typeID2(typeid(trktest::DummyRecord));
    edm::TransitionRecordKey key2(typeID2);
    REQUIRE(key2.name() == "trktest::DummyRecord");
  }

  SECTION("Comparison Operator") {
    edm::TypeIDBase typeID1(typeid(float));
    edm::TypeIDBase typeID2(typeid(float));
    edm::TransitionRecordKey key1(typeID1);
    edm::TransitionRecordKey key2(typeID2);

    REQUIRE(key1 == key2);
    REQUIRE(!(key1 < key2));
    REQUIRE(!(key2 < key1));
    REQUIRE(!(key1 != key2));
  }
  SECTION("MakeKey Method") {
    auto key = edm::TransitionRecordKey::makeKey<int>();
    REQUIRE(key.typeID() == edm::TypeIDBase(typeid(int)));
    REQUIRE(key.name() == "int");
  }
}