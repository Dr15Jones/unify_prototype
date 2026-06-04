#include <tuple>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <concepts>
#include <catch2/catch_all.hpp>
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/IndexedLimitedTaskQueue.h"
#include "Concurrency/SerialTaskQueue.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/chain_first.h"
#include "DataModel/TransitionRecordKey.h"
#include "DataModel/TransitionRecordKeyHash.h"
#include "TransitionHandling/TransitionRecordID.h"
#include "TransitionHandling/ConcurrentTransitionID.h"
#include "TransitionHandling/SourceBase.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "TransitionHandling/TransitionsDistributor.h"
#include "TransitionHandling/AsyncActionBase.h"
#include "oneapi/tbb/task_arena.h"

namespace {
  struct Run {};
  constexpr auto kRunKey = edm::TransitionRecordKey::makeKey<Run>();

  struct Lumi {};
  constexpr auto kLumiKey = edm::TransitionRecordKey::makeKey<Lumi>();

  struct Event {};
  constexpr auto kEventKey = edm::TransitionRecordKey::makeKey<Event>();

  class PrintAction final : public edm::AsyncActionBase {
  public:
    explicit PrintAction(std::string purpose) : purpose_(std::move(purpose)) {}
    void performAsync(edm::WaitingTaskHolder holder,
                      edm::TransitionRecordKey const& key,
                      edm::ConcurrentTransitionID streamID,
                      edm::TransitionRecordID const& recordID) final {
      std::cout << "Performing action " << purpose_ << " for transition " << key.name() << " on stream "
                << streamID.id() << " with record ID of ";
      bool first = true;
      for (auto id : recordID) {
        if (!first) {
          std::cout << "/" << id;
        } else {
          std::cout << id;
          first = false;
        }
      }
      std::cout << std::endl;
      holder.doneWaiting(std::exception_ptr{});
    }
    std::string purpose_;
  };
  class CheckTransitionAction final : public edm::AsyncActionBase {
  public:
    CheckTransitionAction(
        edm::TransitionRecordKey expectedKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>> expectedTransitions)
        : expectedKey_(expectedKey), expectedTransitions_(std::move(expectedTransitions)) {}
    void performAsync(edm::WaitingTaskHolder holder,
                      edm::TransitionRecordKey const& key,
                      edm::ConcurrentTransitionID streamID,
                      edm::TransitionRecordID const& recordID) final {
      REQUIRE(key == expectedKey_);
      REQUIRE(expectedTransitions_.size() > currentIndex_);
      REQUIRE(streamID == expectedTransitions_[currentIndex_].first);
      REQUIRE(recordID == expectedTransitions_[currentIndex_].second);
      ++currentIndex_;
      holder.doneWaiting(std::exception_ptr{});
    }

  private:
    edm::TransitionRecordKey expectedKey_;
    std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>> expectedTransitions_;
    unsigned int currentIndex_ = 0;
  };
}  // namespace

class TestSource final : public edm::SourceBase {
public:
  explicit TestSource(std::vector<edm::SourcePeekResult> transitions)
      : transitions_(std::move(transitions)), current_(-1) {}
  void readTransition(edm::TransitionRecordKey transitionKey, edm::ConcurrentTransitionID streamID) final {
    assert(current_ >= 0);
    assert(current_ < static_cast<int>(transitions_.size()));
    auto const& current = transitions_[current_];
    assert(current.state() == edm::SourceNextState::DataTransition);
    assert(current.recordKey());
    assert(*current.recordKey() == transitionKey);
  }
  void mergeTransition(edm::TransitionRecordKey transitionKey,
                       edm::TransitionRecordID const& recordID,
                       edm::ConcurrentTransitionID streamID) final {
    std::cout << " merging transition " << transitionKey.name() << " with record ID of " << recordID << std::endl;
    auto const& current = transitions_[current_];
    assert(current.state() == edm::SourceNextState::DataTransition);
    assert(current.recordKey());
    assert(*current.recordKey() == transitionKey);
    assert(*current.recordID() == recordID);
  }

  edm::SourcePeekResult goToNextTransition() final {
    ++current_;
    if (current_ < static_cast<int>(transitions_.size())) {
      return transitions_[current_];
    }
    return edm::SourcePeekResult{edm::SourceNextState::Stop};
  }

private:
  std::vector<edm::SourcePeekResult> transitions_;
  int current_;
};

TEST_CASE("Test schedule", "[Schedule]") {
  SECTION("test source") {
    std::vector<edm::SourcePeekResult> transitions{
        edm::SourcePeekResult{edm::SourceNextState::File},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 3U)},
        edm::SourcePeekResult{edm::SourceNextState::Stop},
    };
    edm::ConcurrentTransitionID streamID(0);
    TestSource source(std::move(transitions));
    auto next = source.goToNextTransition();
    REQUIRE(next.state() == edm::SourceNextState::File);
    next = source.goToNextTransition();
    REQUIRE(next.state() == edm::SourceNextState::DataTransition);
    REQUIRE(next.recordKey() == kRunKey);
    REQUIRE(next.recordID() == edm::TransitionRecordID(1U));
    source.readTransition(kRunKey, streamID);
    next = source.goToNextTransition();
    REQUIRE(next.state() == edm::SourceNextState::DataTransition);
    REQUIRE(next.recordKey() == kLumiKey);
    REQUIRE(next.recordID() == edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U));
    source.readTransition(kLumiKey, streamID);
    next = source.goToNextTransition();
    REQUIRE(next.state() == edm::SourceNextState::DataTransition);
    REQUIRE(next.recordKey() == kEventKey);
    REQUIRE(next.recordID() == edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 3U));
    source.readTransition(kEventKey, streamID);
    next = source.goToNextTransition();
    REQUIRE(next.state() == edm::SourceNextState::Stop);
  }
  SECTION("SourceCoordinator") {
    std::vector<edm::SourcePeekResult> transitions{
        edm::SourcePeekResult{edm::SourceNextState::File},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 3U)},
        edm::SourcePeekResult{edm::SourceNextState::Stop},
    };
    edm::ConcurrentTransitionID streamID(0);
    edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
    std::optional<edm::SourcePeekResult> result;
    tbb::task_arena arena(1);
    arena.execute([&coordinator, &result, streamID]() mutable {
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.peekNextTransitionAsync(result, std::move(holder));
        finalTask.wait();
        REQUIRE(result);
        REQUIRE(result->state() == edm::SourceNextState::File);
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.goToNextTransitionAsync(std::move(holder));
        finalTask.wait();
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.peekNextTransitionAsync(result, std::move(holder));
        finalTask.wait();
        REQUIRE(result);
        REQUIRE(result->state() == edm::SourceNextState::DataTransition);
        REQUIRE(result->recordKey() == kRunKey);
        REQUIRE(result->recordID() == edm::TransitionRecordID(1U));
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.readTransitionAsync(kRunKey, streamID, std::move(holder));
        finalTask.wait();
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.peekNextTransitionAsync(result, std::move(holder));
        finalTask.wait();
        REQUIRE(result);
        REQUIRE(result->state() == edm::SourceNextState::DataTransition);
        REQUIRE(result->recordKey() == kLumiKey);
        REQUIRE(result->recordID() == edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U));
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.readTransitionAsync(kLumiKey, streamID, std::move(holder));
        finalTask.wait();
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.peekNextTransitionAsync(result, std::move(holder));
        finalTask.wait();
        REQUIRE(result);
        REQUIRE(result->state() == edm::SourceNextState::DataTransition);
        REQUIRE(result->recordKey() == kEventKey);
        REQUIRE(result->recordID() ==
                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 3U));
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.readTransitionAsync(kEventKey, streamID, std::move(holder));
        finalTask.wait();
      }
      {
        tbb::task_group group;
        edm::FinalWaitingTask finalTask(group);
        edm::WaitingTaskHolder holder(group, &finalTask);
        coordinator.peekNextTransitionAsync(result, std::move(holder));
        finalTask.wait();
        REQUIRE(result);
        REQUIRE(result->state() == edm::SourceNextState::Stop);
      }
    });
  }
  SECTION("TransitionsDistributor and ConcurrentTransitionScheduler") {
    SECTION("just stop") {
      std::vector<edm::SourcePeekResult> transitions{
          edm::SourcePeekResult{edm::SourceNextState::Stop},
      };
      edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
      edm::TransitionsDistributor distributor(coordinator);
      tbb::task_arena arena(1);
      arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
    }
    SECTION("just File transitions") {
      std::vector<edm::SourcePeekResult> transitions{
          edm::SourcePeekResult{edm::SourceNextState::File},
          edm::SourcePeekResult{edm::SourceNextState::File},
          edm::SourcePeekResult{edm::SourceNextState::Stop},
      };
      edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
      edm::TransitionsDistributor distributor(coordinator);
      tbb::task_arena arena(1);
      arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
    }
    SECTION("File and Run Transition") {
      SECTION("one run") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };
        edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        runScheduler.addBeginAction(std::make_unique<PrintAction>("Begin"));
        runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
        runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
        runScheduler.addEndAction(std::make_unique<PrintAction>("End"));
        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
      }
      SECTION("two different runs") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };
        edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        runScheduler.addBeginAction(std::make_unique<PrintAction>("Begin"));
        runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
        runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
        runScheduler.addEndAction(std::make_unique<PrintAction>("End"));
        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
      }
      SECTION("two different files and runs") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };
        edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        runScheduler.addBeginAction(std::make_unique<PrintAction>("Begin"));
        runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
        runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
        runScheduler.addEndAction(std::make_unique<PrintAction>("End"));
        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
      }
      SECTION("two different files and same run") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };
        edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        runScheduler.addBeginAction(std::make_unique<PrintAction>("Begin"));
        runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
        runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
        runScheduler.addEndAction(std::make_unique<PrintAction>("End"));
        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
      }
    }
    SECTION("File, Run and Lumi Transition") {
      std::cout << "Test File, Run and Lumi Transition" << std::endl;
      std::vector<edm::SourcePeekResult> transitions{
          edm::SourcePeekResult{edm::SourceNextState::File},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
          edm::SourcePeekResult{
              edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
          edm::SourcePeekResult{edm::SourceNextState::Stop},
      };
      edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
      edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
      runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
      runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
      runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
      lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
      lumiScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)}}));
      lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin"));
      lumiScheduler.addSupporterBeginAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      lumiScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Lumi stream End"));
      lumiScheduler.addSupporterEndAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
      lumiScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)}}));
      runScheduler.addDependentScheduler(lumiScheduler);
      edm::TransitionsDistributor distributor(coordinator);
      distributor.addSchedulerForTransition(kRunKey, runScheduler);
      distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
      tbb::task_arena arena(1);
      arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
    }
    SECTION("File, Run, Lumi, Run Transition") {
      std::vector<edm::SourcePeekResult> transitions{
          edm::SourcePeekResult{edm::SourceNextState::File},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
          edm::SourcePeekResult{
              edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
          edm::SourcePeekResult{edm::SourceNextState::Stop},
      };
      edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
      edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
      runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
      runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
      runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
      runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
      edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
      lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
      lumiScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)}}));
      lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
      lumiScheduler.addSupporterBeginAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
      lumiScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Lumi stream End Run"));
      lumiScheduler.addSupporterEndAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)}}));
      lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
      lumiScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)}}));
      runScheduler.addDependentScheduler(lumiScheduler);
      edm::TransitionsDistributor distributor(coordinator);
      distributor.addSchedulerForTransition(kRunKey, runScheduler);
      distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
      tbb::task_arena arena(1);
      arena.execute([&distributor]() { REQUIRE_NOTHROW(distributor.processData()); });
    }
    SECTION("File, Run, Lumi, Event, Event, Lumi, Event, Event Transition") {
      std::vector<edm::SourcePeekResult> transitions{
          edm::SourcePeekResult{edm::SourceNextState::File},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
          edm::SourcePeekResult{
              edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                kEventKey,
                                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                kEventKey,
                                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 2U)},
          edm::SourcePeekResult{
              edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                kEventKey,
                                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 1U)},
          edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                kEventKey,
                                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 2U)},
          edm::SourcePeekResult{edm::SourceNextState::Stop},
      };
      edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
      edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
      runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
      runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
      runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kRunKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));

      edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
      lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Begin lumi"));
      lumiScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)}}));
      lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
      lumiScheduler.addSupporterBeginAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      lumiScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Lumi stream End Run"));
      lumiScheduler.addSupporterEndAction(
          kRunKey,
          std::make_unique<CheckTransitionAction>(
              kRunKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)}}));
      lumiScheduler.addEndAction(std::make_unique<PrintAction>("End lumi"));
      lumiScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kLumiKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
              {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)}}));

      edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
      eventScheduler.addBeginAction(std::make_unique<PrintAction>("Begin event"));
      eventScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
          kEventKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 2U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 1U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 2U)}}));
      eventScheduler.addSupporterBeginAction(kLumiKey, std::make_unique<PrintAction>("Event stream Begin Lumi"));
      eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
      eventScheduler.addSupporterBeginAction(
          kLumiKey,
          std::make_unique<CheckTransitionAction>(
              kLumiKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)}}));
      eventScheduler.addSupporterEndAction(kLumiKey, std::make_unique<PrintAction>("Event stream End Lumi"));
      eventScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Event stream End Run"));
      eventScheduler.addSupporterEndAction(
          kLumiKey,
          std::make_unique<CheckTransitionAction>(
              kLumiKey,
              std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
                  {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U)}}));
      eventScheduler.addEndAction(std::make_unique<PrintAction>("End event"));
      eventScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
          kEventKey,
          std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 2U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 1U)},
              {edm::ConcurrentTransitionID(0),
               edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 2U), 2U)}}));

      runScheduler.addDependentScheduler(lumiScheduler);
      lumiScheduler.addDependentScheduler(eventScheduler);

      edm::TransitionsDistributor distributor(coordinator);
      distributor.addSchedulerForTransition(kRunKey, runScheduler);
      distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
      distributor.addSchedulerForTransition(kEventKey, eventScheduler);
      tbb::task_arena arena(1);
      arena.execute([&distributor]() { distributor.processData(); });
    }
  }
  SECTION("File, Run, Lumi, Event, Run, Lumi, Event, Run Transition") {
    std::cout << "Test File, Run, Lumi, Event, Run, Lumi, Event, Run Transition" << std::endl;
    std::vector<edm::SourcePeekResult> transitions{
        edm::SourcePeekResult{edm::SourceNextState::File},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(3U)},
        edm::SourcePeekResult{edm::SourceNextState::Stop},
    };
    edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
    edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
    runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
    runScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
        kRunKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(3U)}}));
    runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
    runScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
        kRunKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(3U)}}));

    edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
    lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Begin lumi"));
    lumiScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
        kLumiKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)}}));
    lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
    lumiScheduler.addSupporterBeginAction(
        kRunKey,
        std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(3U)}}));
    lumiScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Lumi stream End Run"));
    lumiScheduler.addSupporterEndAction(
        kRunKey,
        std::make_unique<CheckTransitionAction>(
            kRunKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(2U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(3U)}}));
    lumiScheduler.addEndAction(std::make_unique<PrintAction>("End lumi"));
    lumiScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
        kLumiKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
            {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)}}));

    edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
    eventScheduler.addBeginAction(std::make_unique<PrintAction>("Begin event"));
    eventScheduler.addBeginAction(std::make_unique<CheckTransitionAction>(
        kEventKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0),
             edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
            {edm::ConcurrentTransitionID(0),
             edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)}}));
    eventScheduler.addSupporterBeginAction(kLumiKey, std::make_unique<PrintAction>("Event stream Begin Lumi"));
    eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
    eventScheduler.addSupporterBeginAction(
        kLumiKey,
        std::make_unique<CheckTransitionAction>(
            kLumiKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)}}));
    eventScheduler.addSupporterEndAction(kLumiKey, std::make_unique<PrintAction>("Event stream End Lumi"));
    eventScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Event stream End Run"));
    eventScheduler.addSupporterEndAction(
        kLumiKey,
        std::make_unique<CheckTransitionAction>(
            kLumiKey,
            std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
                {edm::ConcurrentTransitionID(0), edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)}}));
    eventScheduler.addEndAction(std::make_unique<PrintAction>("End event"));
    eventScheduler.addEndAction(std::make_unique<CheckTransitionAction>(
        kEventKey,
        std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>>{
            {edm::ConcurrentTransitionID(0),
             edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
            {edm::ConcurrentTransitionID(0),
             edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)}}));

    runScheduler.addDependentScheduler(lumiScheduler);
    lumiScheduler.addDependentScheduler(eventScheduler);

    edm::TransitionsDistributor distributor(coordinator);
    distributor.addSchedulerForTransition(kRunKey, runScheduler);
    distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
    distributor.addSchedulerForTransition(kEventKey, eventScheduler);
    tbb::task_arena arena(1);
    arena.execute([&distributor]() { distributor.processData(); });
  }
}