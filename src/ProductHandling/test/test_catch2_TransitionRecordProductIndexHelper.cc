#include <catch2/catch_all.hpp>
#include "ProductHandling/TransitionRecordProductIndexHelper.h"
#include "ProductHandling/TransitionRecordIndexHelpersBuilder.h"
#include "ProductHandling/ProductProviderBundle.h"

namespace prihtest {
  struct DummyRecord {};
  struct DummyProduct {};

  // Mock ProductProviderBundle
  class MockProductProviderBundle : public edm::ProductProviderBundle {
  public:
    void addProductKey(edm::ProductKey key) { keys_.emplace_back(std::move(key)); }

    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>()};
    }

    unsigned int numberOfProvidersForRecord(edm::TransitionRecordKey const& record) const override {
      if (record == edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>()) {
        return 1;
      }
      return 0;
    }
    std::vector<edm::ProductKey> productsFromProvider(edm::TransitionRecordKey const& record, unsigned int providerIndex) const override {
      if (record == edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>() && providerIndex == 0) {
        return {edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess")};
      }
      return {};
    }
  private:
    std::vector<edm::ProductKey> keys_;
  };

}  // namespace prihtest

using namespace prihtest;
TEST_CASE("TransitionRecordIndexHelpersBuilder", "[TransitionRecordIndexHelpersBuilder]") {
  SECTION("Determine Products from Provider") {
    MockProductProviderBundle bundle;  // Assume this is properly implemented
    bundle.addProductKey(
        edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess"));
    edm::TransitionRecordIndexHelpersBuilder builder;

    builder.determineProductsFrom(edm::ProductProviderBundleKey("dummyModule"), bundle);

    auto usedRecords = builder.usedRecords();
    REQUIRE(!usedRecords.empty());
  }
  SECTION("Test no process filling") {
    SECTION("one from current") {
      MockProductProviderBundle bundle;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      bundle.addProductKey(prodKey);
      edm::TransitionRecordIndexHelpersBuilder builder;

      builder.determineProductsFrom(edm::ProductProviderBundleKey("dummyModule"), bundle);
      builder.finalize({"dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::TransitionRecordProductIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::TransitionRecordProductIndex{0});
    }
    SECTION("one from earlier") {
      MockProductProviderBundle bundle;  // Assume this is properly implemented
      auto prodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "dummyProcess");
      bundle.addProductKey(prodKey);
      edm::TransitionRecordIndexHelpersBuilder builder;

      builder.determineProductsFrom(edm::ProductProviderBundleKey("dummyModule"), bundle);
      builder.finalize({"current","dummyProcess"});

      auto helper = builder.helperFor(edm::TransitionRecordKey::makeKey<prihtest::DummyRecord>());
      CHECK(helper->getIndex(prodKey) == edm::TransitionRecordProductIndex{0});
      auto noprodKey = edm::ProductKey::makeKey<prihtest::DummyProduct>("dummyModule", "dummyInstance", "");
      CHECK(helper->getIndex(noprodKey) == edm::TransitionRecordProductIndex{0});
    }
  }
}