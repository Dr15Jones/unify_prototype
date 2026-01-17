#include <catch2/catch.hpp>
#include "oneapi/tbb/global_control.h"
#include "oneapi/tbb/task_group.h"
#include <iostream>
#include "ProductHandling/CrossTransitionProductProvider.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
namespace ctptest {
  struct DummyRecord {};
  struct DummyProduct {};
  struct ParentRecord {};

  class MockProductProvider : public edm::ProductProviderBase {
  public:
    edm::TransitionRecordKey recordForProductsProvided() const override {
      return edm::TransitionRecordKey::makeKey<ParentRecord>();
    }

    std::vector<edm::ProductKey> productsProvided() const override {
      return {edm::ProductKey::makeKey<int>("moduleA", "instanceA", "processA")};
    }

    void provideProductRequestAsync(edm::WaitingTaskHolder task, edm::TransitionContext& context) override {
      // Simulate providing products asynchronously
      task.group()->run([this, task = std::move(task), &context]() {
        std::cout << "Providing products asynchronously" << std::endl;
        // Simulate some processing delay
        // In a real implementation, products would be made available here
        notifyConsumersProductsAvailableAsync(std::move(task), context);
      });
    }
  };

  // Mock ProductConsumer
  class MockProductConsumer : public edm::ProductConsumerBase {
  public:
    edm::TransitionRecordKey reactsToRecord() const final {
      return edm::TransitionRecordKey::makeKey<ctptest::DummyRecord>();
    }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const final {
      return {edm::TransitionRecordKey::makeKey<ctptest::ParentRecord>()};
    }
    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const final {
      return {edm::ProductKey::makeKey<int>("moduleA", "instanceA", "processA")};
    }
    void reactToAllProductsAvailableAsync(edm::WaitingTaskHolder task, edm::TransitionContext& context) final {
      task.group()->run([this, task = std::move(task), &context]() {
        // Simulate processing the available products
        productsAvailableNotified_ = true;
        std::cout << "Products available notified" << std::endl;
      });
    }
    void resetConsumer_() final { productsAvailableNotified_ = false; }

    bool productsAvailableNotified() const { return productsAvailableNotified_; }

  private:
    std::atomic<bool> productsAvailableNotified_ = false;
  };

}  // namespace ctptest
using namespace ctptest;

TEST_CASE("CrossTransitionProductProvider", "[CrossTransition]") {
  oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, 1);

  SECTION("Sub-Transition Product Request") {
    constexpr unsigned int nTransitionInstances = 2;
    MockProductConsumer consumer;

    edm::TransitionContext context;
    edm::TransitionRecordImpl record(edm::TransitionRecordKey::makeKey<ctptest::ParentRecord>(),
                                     std::make_shared<edm::ProductTransitionRecordIndexHelper>(),
                                     0);
    context.insert(record);

    std::vector<MockProductProvider> providers;
    std::vector<edm::ProductProviderBase*> providerPointers;
    providers.reserve(nTransitionInstances);
    for (unsigned int i = 0; i < nTransitionInstances; ++i) {
      providers.emplace_back();
      providerPointers.push_back(&providers.back());
    }

    edm::CrossTransitionProductProvider crossProvider(
      consumer.reactsToRecord(),
        edm::TransitionRecordKey::makeKey<ctptest::ParentRecord>(),
        providerPointers,
        {edm::ProductKey::makeKey<ctptest::DummyProduct>("module", "instance", "process")});

    consumer.addProviderForProducts(&crossProvider);

    {
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      // Request products asynchronously
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(consumer.productsAvailableNotified() == true);
    }
    consumer.resetConsumerForNewTransition();
    crossProvider.resetProvider();
    REQUIRE(consumer.productsAvailableNotified() == false);
    {
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      // Request products asynchronously
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(consumer.productsAvailableNotified() == true);
    }
  }
}