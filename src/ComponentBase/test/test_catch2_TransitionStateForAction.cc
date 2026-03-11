#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "oneapi/tbb/global_control.h"

#include "ComponentBase/TransitionStateForReentrantAction.h"
#include "ComponentBase/TransitionStateForExternalWorkAction.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "ControlFlow/DecisionRequestorBase.h"

namespace {
  struct SimpleAction {
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const {
      return {edm::TransitionRecordKey::makeKey<int>()};
    }
    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const {
      return {edm::ProductKey::makeKey<int>("module", "instance", "process")};
    }
    edm::TransitionRecordKey recordForProductsProvided() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::ProductKey> productsProvided() const {
      return {edm::ProductKey::makeKey<int>("module", "instance", "process")};
    }

    edm::ActionResult work(edm::TransitionContext const& context) {
      // Simulate some work
      return edm::ActionResult(edm::ActionResultStatus::ACCEPT);
    }
  };

  struct SimpleExternalWorkAction {
    std::atomic<bool>* acquireCalled{};
    std::atomic<bool>* workCalled{};
    SimpleExternalWorkAction(std::atomic<bool>* a, std::atomic<bool>* w) : acquireCalled(a), workCalled(w) {}
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const {
      return {edm::TransitionRecordKey::makeKey<int>()};
    }
    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const {
      return {edm::ProductKey::makeKey<int>("module", "instance", "process")};
    }
    edm::TransitionRecordKey recordForProductsProvided() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::ProductKey> productsProvided() const {
      return {edm::ProductKey::makeKey<int>("module", "instance", "process")};
    }

    void acquire(edm::TransitionContext const& context, edm::WaitingTaskHolder holder) {
      // Simulate some asynchronous acquisition
      *acquireCalled = true;
      holder.doneWaiting(std::exception_ptr{});
    }
    edm::ActionResult work(edm::TransitionContext const& context) {
      // Simulate some work
      *workCalled = true;
      return edm::ActionResult(edm::ActionResultStatus::ACCEPT);
    }
  };

  class TrivialConsumer : public edm::ProductConsumerSingleBase<> {
  public:
    std::atomic<bool> wasCalled{false};
    edm::TransitionRecordKey reactsToRecord() const override { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const override {
      return {edm::TransitionRecordKey::makeKey<int>()};
    }
    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const override {
      return {edm::ProductKey::makeKey<int>("module", "instance", "process")};
    }
    void reactToAllProductsAvailableAsync(edm::WaitingTaskHolder task,
                                          edm::TransitionProcessingContext& context) override {
      wasCalled = true;
    }
  };

  class TrvialRequester : public edm::DecisionRequestorBase {
  public:
    std::atomic<bool> wasCalled{false};
    void decisionFromNodeAsync(edm::WaitingTaskHolder,
                               edm::TransitionProcessingContext&,
                               void const*,
                               edm::ControlFlowStatus) final {
      wasCalled = true;
    }
  };
}  // namespace

TEST_CASE("Test TransitionStateForAction", "[TransitionStateForAction]") {
  oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, 1);

  SECTION("Reentrant") {
    SECTION("Simple Action Consumed") {
      edm::TransitionContext context;
      edm::TransitionStateForReentrantAction<SimpleAction> actionState{SimpleAction()};

      edm::TransitionProductProviders providers(actionState.recordForProductsProvided(), {&actionState});
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);
      
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrivialConsumer consumer;
      consumer.addProviderForProducts(actionState.recordForProductsProvided(),
                                      actionState.productsProvided().front(),
                                      providers.indexForProvider(&actionState));
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(consumer.wasCalled.load() == true);
    }

    SECTION("Simple Action Decision") {
      edm::TransitionContext context;
      edm::TransitionStateForReentrantAction<SimpleAction> actionState{SimpleAction()};

      edm::TransitionProductProviders providers(actionState.recordForProductsProvided(), {&actionState});
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrvialRequester requester;
      actionState.addRequestorForDecision(&requester);
      actionState.requestDecisionAsync(
          edm::WaitingTaskHolder(group, &waitTask), processingContext, edm::RequestState::REQUEST_DECISION);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(requester.wasCalled.load() == true);
    }
  }
  SECTION("ExternalWork") {
    SECTION("Simple Action Consumed") {
      edm::TransitionContext context;
      std::atomic<bool> acquireCalled{false};
      std::atomic<bool> workCalled{false};
      edm::TransitionStateForExternalWorkAction<SimpleExternalWorkAction> actionState{
          SimpleExternalWorkAction(&acquireCalled, &workCalled)};

      edm::TransitionProductProviders providers(actionState.recordForProductsProvided(), {&actionState});
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrivialConsumer consumer;
      consumer.addProviderForProducts(actionState.recordForProductsProvided(),
                                      actionState.productsProvided().front(),
                                      providers.indexForProvider(&actionState));
      consumer.requestActionAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(consumer.wasCalled.load() == true);
      REQUIRE(acquireCalled.load() == true);
      REQUIRE(workCalled.load() == true);
    }

    SECTION("Simple Action Decision") {
      edm::TransitionContext context;
      std::atomic<bool> acquireCalled{false};
      std::atomic<bool> workCalled{false};

      edm::TransitionStateForExternalWorkAction<SimpleExternalWorkAction> actionState{
          SimpleExternalWorkAction(&acquireCalled, &workCalled)};

      edm::TransitionProductProviders providers(actionState.recordForProductsProvided(), {&actionState});
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrvialRequester requester;
      actionState.addRequestorForDecision(&requester);
      actionState.requestDecisionAsync(
          edm::WaitingTaskHolder(group, &waitTask), processingContext, edm::RequestState::REQUEST_DECISION);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(requester.wasCalled.load() == true);
      REQUIRE(acquireCalled.load() == true);
      REQUIRE(workCalled.load() == true);
    }
  }
}