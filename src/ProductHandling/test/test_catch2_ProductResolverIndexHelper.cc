#include <catch2/catch.hpp>
#include "ProductHandling/ProductResolverIndexHelper.h"
#include "ProductHandling/ProductResolverIndexHelpersBuilder.h"
#include "ProductHandling/ProductResolversProvider.h"

namespace prihtest {
  struct DummyRecord {};
  struct DummyProduct {};

  // Mock ProductResolversProvider
  class MockProductResolversProvider : public edm::ProductResolversProvider {
  public:
    void addProductKey(edm::ProductKey key) { keys_.emplace_back(std::move(key)); }

    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>()};
    }
    std::vector<edm::ProductKey> productKeysForRecord(edm::TransitionRecordKey const&) const override {
      return {edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess")};
    }

    std::unique_ptr<edm::ProductResolverBase> makeResolver(edm::TransitionRecordKey const&,
                                                           edm::ProductKey const&) const override {
      return nullptr;  // Mock implementation
    }

  private:
    std::vector<edm::ProductKey> keys_;
  };

}  // namespace prihtest

using namespace prihtest;
TEST_CASE("ProductResolverIndexHelpersBuilder", "[ProductResolverIndexHelpersBuilder]") {
  SECTION("Determine Products from Provider") {
    MockProductResolversProvider provider;  // Assume this is properly implemented
    provider.addProductKey(
        edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess"));
    edm::ProductResolverIndexHelpersBuilder builder;

    builder.determineProductsFrom(provider);

    auto usedRecords = builder.usedRecords();
    REQUIRE(!usedRecords.empty());
  }
  SECTION("Test no process filling") {
    SECTION("one from current") {
      MockProductResolversProvider provider;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      provider.addProductKey(prodKey);
      edm::ProductResolverIndexHelpersBuilder builder;

      builder.determineProductsFrom(provider);
      builder.finalize({"dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::ProductResolverIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::ProductResolverIndex{0});
    }
    SECTION("one from earlier") {
      MockProductResolversProvider provider;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      provider.addProductKey(prodKey);
      edm::ProductResolverIndexHelpersBuilder builder;

      builder.determineProductsFrom(provider);
      builder.finalize({"current","dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::ProductResolverIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::ProductResolverIndex{0});
    }
  }
}