#include <catch2/catch.hpp>
#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/ProductProviderBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/FinalWaitingTask.h"
#include "ProductHandling/TransitionContext.h"
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

    void provideProductRequestAsync(edm::WaitingTaskHolder task, edm::TransitionContext& context) override {
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
    void reactToAllProductsAvailableAsync(edm::WaitingTaskHolder task, edm::TransitionContext& context) override {
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
    consumer.addProviderForProducts(&provider);

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }
  SECTION("Two Providers and One Consumer") {
    edm::TransitionContext context;
    MockProductProvider provider1;
    MockProductProvider provider2;
    MockProductConsumer consumer;
    consumer.addProviderForProducts(&provider1);
    consumer.addProviderForProducts(&provider2);

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }

  SECTION("One Provider and Two Consumers") {
    edm::TransitionContext context;
    MockProductProvider provider;
    MockProductConsumer consumer1;
    MockProductConsumer consumer2;
    consumer1.addProviderForProducts(&provider);
    consumer2.addProviderForProducts(&provider);

    oneapi::tbb::task_group group;
    {
      edm::FinalWaitingTask waitTask{group};

      // Request products asynchronously
      consumer1.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
    }
    REQUIRE(consumer1.productsAvailableNotified() == true);
    //since consumer2 has not requested products yet it should not be notified
    REQUIRE(consumer2.productsAvailableNotified() == false);
    {
      edm::FinalWaitingTask waitTask2{group};
      // Request comes after all products are available
      consumer2.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask2), context);
      waitTask2.waitNoThrow();
      REQUIRE(consumer2.productsAvailableNotified() == true);
    }
  }
  SECTION("No Provider and One Consumer") {
    edm::TransitionContext context;
    MockProductConsumer consumer;

    oneapi::tbb::task_group group;
    edm::FinalWaitingTask waitTask{group};

    // Request products asynchronously
    consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
    waitTask.waitNoThrow();
    REQUIRE(consumer.productsAvailableNotified() == true);
  }
}