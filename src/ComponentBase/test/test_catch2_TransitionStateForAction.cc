#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "oneapi/tbb/global_control.h"

#include "ComponentBase/TransitionStateForReentrantAction.h"
#include "ComponentBase/TransitionStateForExternalWorkAction.h"
#include "ComponentBase/TransitionStateForConditionalAction.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "ProductHandling/TransitionContext.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "ProductHandling/ProductTransitionRecordIndexHelper.h"
#include "ProductHandling/ProductsProvider.h"
#include "ProductHandling/TransitionRecordIndexHelpersBuilder.h"
#include "ControlFlow/DecisionRequestorBase.h"
#include "ControlFlow/StartDecisionGraph.h"
#include "DataProductBase/Wrapper.h"

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

  class TrivialRequester : public edm::DecisionRequestorBase {
  public:
    std::atomic<bool> wasCalled{false};
    void decisionFromNodeAsync(edm::WaitingTaskHolder,
                               edm::TransitionProcessingContext&,
                               void const*,
                               edm::ControlFlowStatus) final {
      wasCalled = true;
    }
  };

  struct TriggerResults {
    bool accept = true;
  };

  struct DummyProduct {};
  struct TriggerResultsAction {
    TriggerResults& results_;
    edm::ProductTransitionRecordIndex& putIndex_;

    TriggerResultsAction(TriggerResults& results, edm::ProductTransitionRecordIndex& putIndex)
        : results_(results), putIndex_(putIndex) {}
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const { return {}; }

    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const { return {}; }
    edm::TransitionRecordKey recordForProductsProvided() const { return edm::TransitionRecordKey::makeKey<int>(); }

    std::vector<edm::ProductKey> productsProvided() const {
      return {edm::ProductKey::makeKey<TriggerResults>("triggerResults", "", "process")};
    }

    edm::ActionResult work(edm::TransitionContext& context) {
      // Simulate some work
      context.get(edm::TransitionRecordKey::makeKey<int>())
          ->put(putIndex_, std::make_unique<edm::Wrapper<TriggerResults>>(results_));
      return edm::ActionResult(edm::ActionResultStatus::ACCEPT);
    }
  };

  struct DummyProductAction {
    edm::ProductTransitionRecordIndex& putIndex_;

    DummyProductAction(edm::ProductTransitionRecordIndex& putIndex) : putIndex_(putIndex) {}
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const { return {}; }

    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const { return {}; }
    edm::TransitionRecordKey recordForProductsProvided() const { return edm::TransitionRecordKey::makeKey<int>(); }

    std::vector<edm::ProductKey> productsProvided() const {
      return {edm::ProductKey::makeKey<DummyProduct>("dummy", "", "process")};
    }

    edm::ActionResult work(edm::TransitionContext& context) {
      // Simulate some work
      context.get(edm::TransitionRecordKey::makeKey<int>())
          ->put(putIndex_, std::make_unique<edm::Wrapper<DummyProduct>>(DummyProduct()));
      return edm::ActionResult(edm::ActionResultStatus::ACCEPT);
    }
  };

  struct TriggerResultsFilterAction {
    edm::ProductTransitionRecordIndex& getIndex_;

    TriggerResultsFilterAction(edm::ProductTransitionRecordIndex& getIndex) : getIndex_(getIndex) {}
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const {
      return {edm::TransitionRecordKey::makeKey<int>()};
    }

    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const {
      return {edm::ProductKey::makeKey<TriggerResults>("triggerResults", "", "process")};
    }
    edm::TransitionRecordKey recordForProductsProvided() const { return {}; }

    std::vector<edm::ProductKey> productsProvided() const { return {}; }

    edm::ActionResult work(edm::TransitionContext& context) {
      // Simulate some work
      auto wrapper = context.get(edm::TransitionRecordKey::makeKey<int>())->get(getIndex_);
      assert(wrapper);
      auto const& triggerResults = dynamic_cast<edm::Wrapper<TriggerResults> const*>(wrapper)->product();
      return edm::ActionResult(triggerResults.accept ? edm::ActionResultStatus::ACCEPT
                                                     : edm::ActionResultStatus::REJECT);
    }
  };

  struct WriteAction {
    bool& wasCalled_;
    edm::ProductTransitionRecordIndex& getIndex_;

    WriteAction(bool& wasCalled, edm::ProductTransitionRecordIndex& getIndex)
        : wasCalled_(wasCalled), getIndex_(getIndex) {}
    edm::TransitionRecordKey reactsToRecord() const { return edm::TransitionRecordKey::makeKey<int>(); }
    std::vector<edm::TransitionRecordKey> recordForProductsConsumed() const {
      return {edm::TransitionRecordKey::makeKey<int>()};
    }

    std::vector<edm::ProductKey> productsConsumed(edm::TransitionRecordKey const&) const {
      return {edm::ProductKey::makeKey<DummyProduct>("dummy", "", "process")};
    }
    edm::TransitionRecordKey recordForProductsProvided() const { return {}; }

    std::vector<edm::ProductKey> productsProvided() const { return {}; }

    edm::ActionResult work(edm::TransitionContext& context) {
      // Simulate some work
      wasCalled_ = true;
      return edm::ActionResult(edm::ActionResultStatus::ACCEPT);
    }
  };

  struct SimpleProductProviders : public edm::ProductsProvider {
    SimpleProductProviders(edm::TransitionRecordKey key, std::vector<edm::ProductKey> products)
        : key_(key), products_(products) {}
    std::vector<edm::TransitionRecordKey> resolverRecords() const final { return {key_}; }
    unsigned int numberOfProvidersForRecord(edm::TransitionRecordKey const& record) const final {
      return (record == key_) ? 1 : 0;
    }
    std::vector<edm::ProductKey> productsFromProvider(edm::TransitionRecordKey const& record, unsigned int providerIndex) const final {
      if (record != key_) {
        return {};
      }
      if (providerIndex != 0) {
        return {};
      }
      return products_;
    }
    edm::TransitionRecordKey key_;
    std::vector<edm::ProductKey> products_;
  };

}  // namespace

TEST_CASE("Test TransitionStateForAction", "[TransitionStateForAction]") {
  oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, 1);

  SECTION("Reentrant") {
    SECTION("Simple Action Consumed") {
      edm::TransitionContext context;
      edm::TransitionStateForReentrantAction<SimpleAction> actionState{SimpleAction()};

      edm::TransitionProductProviders providers(actionState.recordForProductsProvided(), {&actionState});
      const edm::TransitionProductProviderIndex actionStateIndex{0};
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrivialConsumer consumer;
      consumer.addProviderForProducts(actionState.recordForProductsProvided(),
                                      actionState.productsProvided().front(),
                                      actionStateIndex);
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

      TrivialRequester requester;
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
      const edm::TransitionProductProviderIndex actionStateIndex{0};
      edm::TransitionProviderContext providerContext;
      providerContext.insert(providers);

      edm::TransitionProcessingContext processingContext(context, providerContext);

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      TrivialConsumer consumer;
      consumer.addProviderForProducts(actionState.recordForProductsProvided(),
                                      actionState.productsProvided().front(),
                                      actionStateIndex);
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

      TrivialRequester requester;
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
  SECTION("Conditional") {
    edm::TransitionContext context;
    TriggerResults results;
    edm::ProductTransitionRecordIndex triggerIndex;
    edm::TransitionStateForReentrantAction<TriggerResultsAction> triggerAction{results, triggerIndex};

    edm::ProductTransitionRecordIndex dummyIndex;
    edm::TransitionStateForReentrantAction<DummyProductAction> dummyAction{dummyIndex};

    SimpleProductProviders triggerProviders(triggerAction.recordForProductsProvided(),
                                            triggerAction.productsProvided());
    SimpleProductProviders dummyProviders(dummyAction.recordForProductsProvided(), dummyAction.productsProvided());
    edm::TransitionRecordIndexHelpersBuilder builder;
    builder.determineProductsFrom(edm::ProvidersKey("Trigger"), triggerProviders);
    builder.determineProductsFrom(edm::ProvidersKey("Dummy"), dummyProviders);
    builder.finalize({"process"});

    auto helper = builder.helperFor(triggerAction.recordForProductsProvided());
    triggerIndex = helper->getIndex(triggerAction.productsProvided().front());
    dummyIndex = helper->getIndex(dummyAction.productsProvided().front());
    edm::TransitionRecordImpl record(edm::TransitionRecordKey::makeKey<int>(), helper, 0);
    context.insert(record);

    edm::TransitionProductProviders providers(triggerAction.recordForProductsProvided(),
                                              {&triggerAction, &dummyAction});
    const edm::TransitionProductProviderIndex triggerProvIndex{0};
    const edm::TransitionProductProviderIndex dummyProvIndex{1};

    edm::TransitionProviderContext providerContext;
    providerContext.insert(providers);

    edm::TransitionProcessingContext processingContext(context, providerContext);

    auto filter = std::make_unique<edm::TransitionStateForReentrantAction<TriggerResultsFilterAction>>(triggerIndex);
    filter->addProviderForProducts(
        triggerAction.recordForProductsProvided(), triggerAction.productsProvided().front(), triggerProvIndex);
    bool writeWasCalled = false;
    auto write = std::make_unique<edm::TransitionStateForReentrantAction<WriteAction>>(writeWasCalled, dummyIndex);
    write->addProviderForProducts(dummyAction.recordForProductsProvided(),
                                  dummyAction.productsProvided().front(),
                                  dummyProvIndex);
    edm::TransitionStateForConditionalAction conditionState{std::move(filter), std::move(write)};

    edm::StartDecisionGraph startGraph;
    startGraph.addLeafNode(&conditionState);
    {
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};
      startGraph.startAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(writeWasCalled == true);
    }
    {
      writeWasCalled = false;
      results.accept = false;
      triggerAction.resetForNewTransition();
      dummyAction.resetForNewTransition();
      conditionState.resetForNewTransition();
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};
      startGraph.startAsync(edm::WaitingTaskHolder(group, &waitTask), processingContext);
      waitTask.waitNoThrow();
      REQUIRE(waitTask.done());
      REQUIRE(not waitTask.exceptionPtr());
      REQUIRE(writeWasCalled == false);
    }
  }
}