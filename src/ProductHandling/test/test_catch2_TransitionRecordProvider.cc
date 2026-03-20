#include <catch2/catch.hpp>

#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductProviderBundle.h"
#include "ProductHandling/TransitionRecordIndexHelpersBuilder.h"
#include "DataModel/TransitionRecordKey.h"

namespace trptest {
  struct DummyRecord {};

  // Mock ProductProviderBundle
  class MockProductProviderBundle : public edm::ProductProviderBundle {
  public:
    std::vector<edm::TransitionRecordKey> resolverRecords() const override {
      return {edm::TransitionRecordKey::makeKey<trptest::DummyRecord>()};
    }
    unsigned int numberOfProvidersForRecord(edm::TransitionRecordKey const& record) const override {
      return (record == edm::TransitionRecordKey::makeKey<trptest::DummyRecord>()) ? 1 : 0;
    }
    std::vector<edm::ProductKey> productsFromProvider(edm::TransitionRecordKey const& record, unsigned int providerIndex) const override {
      return {};
    }
  };

}  // namespace trptest

TEST_CASE("TransitionRecordProvider", "[TransitionRecordProvider]") {
  SECTION("Constructor and Key Access") {
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key,std::make_shared<edm::TransitionRecordProductIndexHelper>(), 1);

    REQUIRE(provider.key() == key);
  }
  SECTION("Add Resolvers from Provider") {
    edm::TransitionRecordIndexHelpersBuilder builder;
    trptest::MockProductProviderBundle mockProvider;
    builder.determineProductsFrom(edm::ProductProviderBundleKey("mock"), mockProvider);
    edm::TransitionRecordKey key = edm::TransitionRecordKey::makeKey<trptest::DummyRecord>();
    edm::TransitionRecordProvider provider(key, builder.helperFor(key), 1);

    // Further checks can be added here to verify the resolvers were added correctly
  }
}