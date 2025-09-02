#include <catch2/catch.hpp>

#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductResolversProvider.h"
#include "ProductHandling/ProductResolverIndexHelpersBuilder.h"
#include "DataModel/TransitionRecordKey.h"

namespace trptest {
  struct DummyRecord {};

  // Mock ProductResolversProvider
  class MockProductResolversProvider : public edm::ProductResolversProvider {
  public:
    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<trptest::DummyRecord>()};
    }
    std::vector<edm::ProductKey> productKeysForRecord(edm::TransitionRecordKey const&) const override { return {}; }
    std::unique_ptr<edm::ProductResolverBase> makeResolver(edm::TransitionRecordKey const&,
                                                           edm::ProductKey const&) const override {
      return nullptr;  // Mock implementation
    }
  };

}  // namespace trptest

TEST_CASE("TransitionRecordProvider", "[TransitionRecordProvider]") {
  SECTION("Constructor and Key Access") {
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key,std::make_shared<edm::ProductResolverIndexHelper>(), 1);

    REQUIRE(provider.key() == key);
  }
  SECTION("Add Resolvers from Provider") {
    edm::ProductResolverIndexHelpersBuilder builder;
    trptest::MockProductResolversProvider mockProvider;
    builder.determineProductsFrom(mockProvider);
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key, builder.helperFor(key), 1);

    provider.addResolversFrom(mockProvider);

    // Further checks can be added here to verify the resolvers were added correctly
  }
}