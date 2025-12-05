#include <catch2/catch.hpp>

#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductsProvider.h"
#include "ProductHandling/ProductTransitionRecordIndexHelpersBuilder.h"
#include "DataModel/TransitionRecordKey.h"

namespace trptest {
  struct DummyRecord {};

  // Mock ProductsProvider
  class MockProductsProvider : public edm::ProductsProvider {
  public:
    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<trptest::DummyRecord>()};
    }
    std::vector<edm::ProductKey> productKeysForRecord(edm::TransitionRecordKey const&) const override { return {}; }
  };

}  // namespace trptest

TEST_CASE("TransitionRecordProvider", "[TransitionRecordProvider]") {
  SECTION("Constructor and Key Access") {
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key,std::make_shared<edm::ProductTransitionRecordIndexHelper>(), 1);

    REQUIRE(provider.key() == key);
  }
  SECTION("Add Resolvers from Provider") {
    edm::ProductTransitionRecordIndexHelpersBuilder builder;
    trptest::MockProductsProvider mockProvider;
    builder.determineProductsFrom(mockProvider);
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key, builder.helperFor(key), 1);

    provider.addResolversFrom(mockProvider);

    // Further checks can be added here to verify the resolvers were added correctly
  }
}