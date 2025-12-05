#include <catch2/catch.hpp>
#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
#include "ProductHandling/ProductTransitionRecordIndexHelpersBuilder.h"
#include "ProductHandling/ProductsProvider.h"

namespace prihtest {
  struct DummyRecord {};
  struct DummyProduct {};

  // Mock ProductsProvider
  class MockProductsProvider : public edm::ProductsProvider {
  public:
    void addProductKey(edm::ProductKey key) { keys_.emplace_back(std::move(key)); }

    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>()};
    }
    std::vector<edm::ProductKey> productKeysForRecord(edm::TransitionRecordKey const&) const override {
      return {edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess")};
    }

  private:
    std::vector<edm::ProductKey> keys_;
  };

}  // namespace prihtest

using namespace prihtest;
TEST_CASE("ProductTransitionRecordIndexHelpersBuilder", "[ProductTransitionRecordIndexHelpersBuilder]") {
  SECTION("Determine Products from Provider") {
    MockProductsProvider provider;  // Assume this is properly implemented
    provider.addProductKey(
        edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess"));
    edm::ProductTransitionRecordIndexHelpersBuilder builder;

    builder.determineProductsFrom(provider);

    auto usedRecords = builder.usedRecords();
    REQUIRE(!usedRecords.empty());
  }
  SECTION("Test no process filling") {
    SECTION("one from current") {
      MockProductsProvider provider;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      provider.addProductKey(prodKey);
      edm::ProductTransitionRecordIndexHelpersBuilder builder;

      builder.determineProductsFrom(provider);
      builder.finalize({"dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::ProductTransitionRecordIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::ProductTransitionRecordIndex{0});
    }
    SECTION("one from earlier") {
      MockProductsProvider provider;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      provider.addProductKey(prodKey);
      edm::ProductTransitionRecordIndexHelpersBuilder builder;

      builder.determineProductsFrom(provider);
      builder.finalize({"current","dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::ProductTransitionRecordIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::ProductTransitionRecordIndex{0});
    }
  }
}