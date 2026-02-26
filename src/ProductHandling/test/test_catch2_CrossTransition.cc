#include <catch2/catch.hpp>
#include "oneapi/tbb/global_control.h"
#include "oneapi/tbb/task_group.h"
#include <iostream>
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ProductHandling/ProductProviderBase.h"
#include "ProductHandling/ProductConsumerBase.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
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

    void provideProductRequestAsync(edm::WaitingTaskHolder task, edm::TransitionProcessingContext const & context) override {
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
    void reactToAllProductsAvailableAsync(edm::WaitingTaskHolder task, edm::TransitionProcessingContext const & context) final {
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
    std::vector<edm::TransitionProductProviders> providersRecords;
    auto const& recordKey = record.key();
    providers.reserve(nTransitionInstances);
    providersRecords.reserve(nTransitionInstances);
    for (unsigned int i = 0; i < nTransitionInstances; ++i) {
      providers.emplace_back();
      providersRecords.emplace_back(recordKey, std::vector<edm::ProductProviderBase*>{&providers.back()});
    }

    consumer.addProviderForProducts(recordKey, providersRecords.front().indexForProvider(&providers.front()));
    {
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::TransitionProviderContext providerContext;
      providerContext.insert(providersRecords.front());
      edm::TransitionProcessingContext processingContext{context, providerContext};

      // Request products asynchronously
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(consumer.productsAvailableNotified() == true);
    }
    consumer.resetConsumerForNewTransition();
    REQUIRE(consumer.productsAvailableNotified() == false);
    {
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::TransitionProviderContext providerContext;
      providerContext.insert(providersRecords.front());
      edm::TransitionProcessingContext processingContext{context, providerContext};

      // Request products asynchronously
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(consumer.productsAvailableNotified() == true);
    }
  }
}