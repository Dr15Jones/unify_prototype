#include <catch2/catch.hpp>
#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/ProductProviderBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/FinalWaitingTask.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "oneapi/tbb/global_control.h"

namespace pcptest {
  struct DummyRecord {};

  // Mock ProductProvider
  class MockProductProvider : public edm::ProductProviderBase {
  public:
    edm::TransitionRecordKey recordForProductsProvided() const override {
      return edm::TransitionRecordKey::makeKey<pcptest::DummyRecord>();
    }

    std::vector<edm::ProductKey> productsProvided() const override {
      return {edm::ProductKey::makeKey<int>("moduleA", "instanceA", "processA")};
    }

    void provideProductRequestAsync(edm::WaitingTaskHolder task, edm::TransitionProcessingContext const & context) override {
      // Simulate providing products asynchronously
      task.group()->run([this, task = std::move(task), &context]() {
        // Simulate some processing delay
        // In a real implementation, products would be made available here
        notifyConsumersProductsAvailableAsync(std::move(task), context);
      });
    }
  };

  // Mock ProductConsumer
  class MockProductConsumer : public edm::ProductConsumerBase {
  public:
    edm::TransitionRecordKey reactsToRecord() const override {
      return edm::TransitionRecordKey::makeKey<pcptest::DummyRecord>();
    }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const override {
      return {edm::TransitionRecordKey::makeKey<pcptest::DummyRecord>()};
    }
    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const override {
      return {edm::ProductKey::makeKey<int>("moduleA", "instanceA", "processA")};
    }
    void reactToAllProductsAvailableAsync(edm::WaitingTaskHolder task, edm::TransitionProcessingContext const & context) override {
      task.group()->run([this, task = std::move(task), &context]() {
        // Simulate processing the available products
        productsAvailableNotified_ = true;
      });
      // Further processing can be simulated here
    }

    bool productsAvailableNotified() const { return productsAvailableNotified_; }

  private:
    std::atomic<bool> productsAvailableNotified_ = false;
  };
}  // namespace pcptest

using namespace pcptest;
TEST_CASE("ProductConsumerBase and ProductProviderBase Interaction", "[ProductConsumerBase][ProductProviderBase]") {
  oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, 1);
  SECTION("One Provider and One Consumer") {
    edm::TransitionContext context;
    MockProductProvider provider;
    MockProductConsumer consumer;
    edm::TransitionProviderContext providerContext;
    edm::TransitionProductProviders providers{provider.recordForProductsProvided(), {&provider}};
    providerContext.insert(providers);
    edm::TransitionProcessingContext processingContext{context, providerContext};
    consumer.addProviderForProducts(provider.recordForProductsProvided(), providers.indexForProvider(&provider));

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }
  SECTION("Two Providers and One Consumer") {
    edm::TransitionContext context;
    MockProductProvider provider1;
    MockProductProvider provider2;
    MockProductConsumer consumer;
    edm::TransitionProviderContext providerContext;
    edm::TransitionProductProviders providers{provider1.recordForProductsProvided(), {&provider1, &provider2}};
    REQUIRE(provider1.recordForProductsProvided() == provider2.recordForProductsProvided());
    providerContext.insert(providers);
    edm::TransitionProcessingContext processingContext{context, providerContext};

    consumer.addProviderForProducts(provider1.recordForProductsProvided(), providers.indexForProvider(&provider1));
    consumer.addProviderForProducts(provider2.recordForProductsProvided(), providers.indexForProvider(&provider2));

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }

  SECTION("One Provider and Two Consumers") {
    edm::TransitionContext context;
    MockProductProvider provider;
    MockProductConsumer consumer1;
    MockProductConsumer consumer2;
    edm::TransitionProviderContext providerContext;
    edm::TransitionProductProviders providers{provider.recordForProductsProvided(), {&provider}};
    providerContext.insert(providers);
    edm::TransitionProcessingContext processingContext{context, providerContext};

    consumer1.addProviderForProducts(provider.recordForProductsProvided(), providers.indexForProvider(&provider));
    consumer2.addProviderForProducts(provider.recordForProductsProvided(), providers.indexForProvider(&provider));

    oneapi::tbb::task_group group;
    {
      edm::FinalWaitingTask waitTask{group};

      // Request products asynchronously
      consumer1.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
    }
    REQUIRE(consumer1.productsAvailableNotified() == true);
    //since consumer2 has not requested products yet it should not be notified
    REQUIRE(consumer2.productsAvailableNotified() == false);
    {
      edm::FinalWaitingTask waitTask2{group};
      // Request comes after all products are available
      consumer2.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask2), processingContext);
      waitTask2.waitNoThrow();
      REQUIRE(consumer2.productsAvailableNotified() == true);
    }
  }
  SECTION("No Provider and One Consumer") {
    edm::TransitionContext context;
    MockProductConsumer consumer;

    edm::TransitionProviderContext providerContext;
    edm::TransitionProcessingContext processingContext{context, providerContext};

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }
}