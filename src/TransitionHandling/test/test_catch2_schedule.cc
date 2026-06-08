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
#ifndef NDEBUG
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
#endif
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
      try {
        REQUIRE(key == expectedKey_);
        REQUIRE(expectedTransitions_.size() > currentIndex_);
        REQUIRE(streamID == expectedTransitions_[currentIndex_].first);
        REQUIRE(recordID == expectedTransitions_[currentIndex_].second);
        ++currentIndex_;
        holder.doneWaiting(std::exception_ptr{});
      } catch (...) {
        holder.doneWaiting(std::current_exception());
      }
    }

  private:
    edm::TransitionRecordKey expectedKey_;
    std::vector<std::pair<edm::ConcurrentTransitionID, edm::TransitionRecordID>> expectedTransitions_;
    unsigned int currentIndex_ = 0;
  };

  class CheckTransitionOrderAction final : public edm::AsyncActionBase {
  public:
    CheckTransitionOrderAction(edm::TransitionRecordKey expectedKey,
                               std::vector<unsigned int> expectedCounts,
                               unsigned int& counter)
        : expectedKey_(expectedKey), expectedCounts_(expectedCounts), counter_(counter) {}
    void performAsync(edm::WaitingTaskHolder holder,
                      edm::TransitionRecordKey const& key,
                      edm::ConcurrentTransitionID streamID,
                      edm::TransitionRecordID const& recordID) final {
      try {
        REQUIRE(key == expectedKey_);
        REQUIRE(timesCalled_ < expectedCounts_.size());
        REQUIRE(expectedCounts_[timesCalled_] == counter_);
        ++counter_;
        ++timesCalled_;
        holder.doneWaiting(std::exception_ptr{});
      } catch (...) {
        holder.doneWaiting(std::current_exception());
      }
    }

  private:
    edm::TransitionRecordKey expectedKey_;
    std::vector<unsigned int> expectedCounts_;
    unsigned int timesCalled_ = 0;
    unsigned int& counter_;
  };

  class ThrowAction final : public edm::AsyncActionBase {
  public:
    ThrowAction(std::string message, unsigned int countThreshold)
        : message_(std::move(message)), countThreshold_(countThreshold) {}
    void performAsync(edm::WaitingTaskHolder holder,
                      edm::TransitionRecordKey const& key,
                      edm::ConcurrentTransitionID streamID,
                      edm::TransitionRecordID const& recordID) final {
      auto count = counter_++;
      if (count == countThreshold_) {
        //std::cout << "** Throwing exception for " << key.name() << " on stream " << streamID.id() << " at count "
                  //<< count << std::endl;
        auto fullMessage = message_ + " at count " + std::to_string(count) + " " + key.name() + " for stream " +
                           std::to_string(streamID.id());
        holder.doneWaiting(std::make_exception_ptr(std::runtime_error(fullMessage)));
      } else if (count > countThreshold_) {
        auto fullMessage = "DID NOT STOP AT THRESHOLD (" + message_ + ") at count " + std::to_string(count) + " " +
                           key.name() + " for stream " + std::to_string(streamID.id());
        REQUIRE(fullMessage == "FAIL");
      } else {
        holder.doneWaiting(std::exception_ptr{});
      }
    }
    std::string message_;
    unsigned int countThreshold_;
    unsigned int counter_ = 0;
  };

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
      //std::cout << " merging transition " << transitionKey.name() << " with record ID of " << recordID << std::endl;
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

  class ThrowingTestSource final : public edm::SourceBase {
  public:
    enum class ThrowAt { kGoToNext, kRead, kMerge };
    explicit ThrowingTestSource(std::vector<edm::SourcePeekResult> transitions, ThrowAt throwAt, int throwWhenAtIndex)
        : transitions_(std::move(transitions)), current_(-1), throwAt_(throwAt), throwWhenAtIndex_(throwWhenAtIndex) {}
    void readTransition(edm::TransitionRecordKey transitionKey, edm::ConcurrentTransitionID streamID) final {
      if (throwAt_ == ThrowAt::kRead) {
        ++indexAt_;
        if (throwWhenAtIndex_ == indexAt_) {
          throw std::runtime_error("Error in readTransition");
        }
      }
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
      //std::cout << " merging transition " << transitionKey.name() << " with record ID of " << recordID << std::endl;
      if (throwAt_ == ThrowAt::kMerge) {
        ++indexAt_;
        if (throwWhenAtIndex_ == indexAt_) {
          throw std::runtime_error("Error in mergeTransition");
        }
      }
      auto const& current = transitions_[current_];
      assert(current.state() == edm::SourceNextState::DataTransition);
      assert(current.recordKey());
      assert(*current.recordKey() == transitionKey);
      assert(*current.recordID() == recordID);
    }

    edm::SourcePeekResult goToNextTransition() final {
      ++current_;
      if (throwAt_ == ThrowAt::kGoToNext) {
        ++indexAt_;
        if (indexAt_ == throwWhenAtIndex_) {
          //std::cout << "** Throwing exception in goToNextTransition at index " << current_ << std::endl;
          throw std::runtime_error("Error in goToNextTransition");
        } else if (throwWhenAtIndex_ < indexAt_) {
          //std::cout << "** DID NOT STOP THROWING in goToNextTransition at index " << current_ << std::endl;
          auto const* kFail = "goToNext should have thrown";
          abort();
          REQUIRE(not kFail);
        }
      }
      if (current_ < static_cast<int>(transitions_.size())) {
        return transitions_[current_];
      }
      return edm::SourcePeekResult{edm::SourceNextState::Stop};
    }

  private:
    std::vector<edm::SourcePeekResult> transitions_;
    int current_;
    int indexAt_ = -1;
    const ThrowAt throwAt_;
    const int throwWhenAtIndex_;
  };

}  // namespace

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
  SECTION("No Suppporter actions") {
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
  SECTION("Test supporter end ordering") {
    std::vector<edm::SourcePeekResult> transitions{
        edm::SourcePeekResult{edm::SourceNextState::File},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
        //Tests that new Run triggers supporter end Lumi and Run in correct order
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)},
        //Tests that ending job triggers supporter end Lumi and Run in correct order
        edm::SourcePeekResult{edm::SourceNextState::Stop},
    };

    unsigned int counter = 0;
    edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
    edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
    runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
    runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
    edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
    lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Begin lumi"));
    lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
    lumiScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Lumi stream End Run"));
    lumiScheduler.addEndAction(std::make_unique<PrintAction>("End lumi"));
    edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
    eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
    eventScheduler.addSupporterBeginAction(
        kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0U, 6U}, counter));
    eventScheduler.addSupporterBeginAction(kLumiKey, std::make_unique<PrintAction>("Event stream Begin Lumi"));
    eventScheduler.addSupporterBeginAction(
        kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{1U, 7U}, counter));
    eventScheduler.addBeginAction(std::make_unique<PrintAction>("Begin event"));
    eventScheduler.addBeginAction(
        std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{2U, 8U}, counter));
    eventScheduler.addEndAction(std::make_unique<PrintAction>("End event"));
    eventScheduler.addEndAction(
        std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{3U, 9U}, counter));
    eventScheduler.addSupporterEndAction(kLumiKey, std::make_unique<PrintAction>("Event stream End Lumi"));
    eventScheduler.addSupporterEndAction(
        kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{4U, 10U}, counter));
    eventScheduler.addSupporterEndAction(kRunKey, std::make_unique<PrintAction>("Event stream End Run"));
    eventScheduler.addSupporterEndAction(
        kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{5U, 11U}, counter));

    runScheduler.addDependentScheduler(lumiScheduler);
    lumiScheduler.addDependentScheduler(eventScheduler);

    edm::TransitionsDistributor distributor(coordinator);
    distributor.addSchedulerForTransition(kRunKey, runScheduler);
    distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
    distributor.addSchedulerForTransition(kEventKey, eventScheduler);
    tbb::task_arena arena(1);
    arena.execute([&distributor]() { distributor.processData(); });
    REQUIRE(counter == 12U);
  }
  SECTION("Exceptions") {
    std::vector<edm::SourcePeekResult> transitions{
        edm::SourcePeekResult{edm::SourceNextState::File},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
        edm::SourcePeekResult{
            edm::SourceNextState::DataTransition, kLumiKey, edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
        edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                              kEventKey,
                              edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
        //should not get here
        edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
        edm::SourcePeekResult{edm::SourceNextState::Stop},
    };

    SECTION("from  action") {
      SECTION("in transition") {
        SECTION("begin Run") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Run", 0U));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }
        SECTION("end Run") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Run", 0U));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{10U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{7U}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{8U}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3}, counter));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{4U}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{5U}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{6U}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{9U}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }

        SECTION("begin Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{6U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0U}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Lumi", 0U));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3U}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{5U}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }
        SECTION("end Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{10U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
          lumiScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Lumi", 0U));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{7U}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{8U}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3}, counter));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{4U}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{5U}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{6U}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{9U}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }

        SECTION("begin Event") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{10U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{7U}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{8U}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3}, counter));
          eventScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Event", 0U));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{4U}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{5U}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{6U}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{9U}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }

        SECTION("end Event") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{10U}, counter));

          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
          lumiScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{7U}, counter));
          lumiScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{8U}, counter));
          runScheduler.addDependentScheduler(lumiScheduler);

          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
          eventScheduler.addSupporterBeginAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
          eventScheduler.addSupporterBeginAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3}, counter));
          eventScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Event", 0U));
          eventScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{4U}, counter));
          eventScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{5U}, counter));
          eventScheduler.addSupporterEndAction(
              kLumiKey, std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{6U}, counter));
          eventScheduler.addSupporterEndAction(
              kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{9U}, counter));
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }
      }
      //
      SECTION("in transition without supporter actions") {
        SECTION("begin Run") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          unsigned int counter = 0;
          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            runScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Run", 0U));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
              {
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
            }
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0U}, counter));
          }

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 1U);
        }
        SECTION("end Run") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          unsigned int counter = 0;
          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{0U}, counter));
              {
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{1U}, counter));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{2U}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3U}, counter));
            }
            runScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Run", 0U));
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          }

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);
          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 5U);
        }

        SECTION("begin Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));

          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          unsigned int counter = 0;
          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Lumi", 0U));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{0U}, counter));
              {
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{1U}, counter));
            }
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{2U}, counter));
          }

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 3U);
        }
        SECTION("end Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{0U}, counter));
              {
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{1U}, counter));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{2U}, counter));
              }
              lumiScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Lumi", 0U));
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3U}, counter));
            }
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          }

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 5U);
        }

        SECTION("begin Event") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          unsigned int counter = 0;
          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{0U}, counter));
              {
                eventScheduler.addBeginAction(std::make_unique<ThrowAction>("Exception in begin Event", 0U));
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{1U}, counter));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{2U}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3U}, counter));
            }
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          }

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 5U);
        }

        SECTION("end Event") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          unsigned int counter = 0;
          {
            runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{0U}, counter));
              {
                eventScheduler.addBeginAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{1U}, counter));
                eventScheduler.addEndAction(std::make_unique<ThrowAction>("Exception in end Event", 0U));
                eventScheduler.addEndAction(
                    std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{2U}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3U}, counter));
            }
            runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
            runScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          }

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
          REQUIRE(counter == 5U);
        }
      }
      SECTION("in supporter") {
        SECTION("begin Run supporter for Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          runScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0U}, counter));
          {
            lumiScheduler.addSupporterBeginAction(
                kRunKey, std::make_unique<ThrowAction>("Exception in begin Run supporter for Lumi", 0U));
            lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
            lumiScheduler.addSupporterBeginAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
            eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
            eventScheduler.addSupporterBeginAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{2}, counter));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
              {
                eventScheduler.addSupporterBeginAction(
                    kLumiKey,
                    std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
                {
                  eventScheduler.addBeginAction(
                      std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
                  eventScheduler.addEndAction(
                      std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
                }
                eventScheduler.addSupporterEndAction(
                    kLumiKey,
                    std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
            }
            lumiScheduler.addSupporterEndAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{3U}, counter));
            eventScheduler.addSupporterEndAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{4U}, counter));
          }
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{5U}, counter));

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }
        SECTION("end Run supporter for Lumi") {
          edm::SourceCoordinator coordinator(std::make_unique<TestSource>(std::move(transitions)));
          unsigned int counter = 0;
          edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
          edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
          runScheduler.addDependentScheduler(lumiScheduler);
          edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
          lumiScheduler.addDependentScheduler(eventScheduler);

          edm::TransitionsDistributor distributor(coordinator);
          distributor.addSchedulerForTransition(kRunKey, runScheduler);
          distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
          distributor.addSchedulerForTransition(kEventKey, eventScheduler);

          runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
          {
            lumiScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Lumi stream Begin Run"));
            lumiScheduler.addSupporterBeginAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
            eventScheduler.addSupporterBeginAction(kRunKey, std::make_unique<PrintAction>("Event stream Begin Run"));
            eventScheduler.addSupporterBeginAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1U}, counter));
            {
              lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
              lumiScheduler.addBeginAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2U}, counter));
              {
                eventScheduler.addSupporterBeginAction(
                    kLumiKey,
                    std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{3}, counter));
                {
                  eventScheduler.addBeginAction(
                      std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{4U}, counter));
                  eventScheduler.addEndAction(
                      std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{5U}, counter));
                }
                eventScheduler.addSupporterEndAction(
                    kLumiKey,
                    std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{6U}, counter));
              }
              lumiScheduler.addEndAction(
                  std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{7U}, counter));
            }
            lumiScheduler.addSupporterEndAction(
                kRunKey, std::make_unique<ThrowAction>("Exception in end Run supporter for Lumi", 0U));
            lumiScheduler.addSupporterEndAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{8U}, counter));
            eventScheduler.addSupporterEndAction(
                kRunKey, std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{9U}, counter));
          }
          runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
          runScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{10U}, counter));

          tbb::task_arena arena(1);
          arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        }
      }
    }
    SECTION("from source") {
      SECTION("peek for File") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kGoToNext, 0));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
      }
      SECTION("peek for Run") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kGoToNext, 1));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
      }
      SECTION("peek for Lumi") {
        //peek 0: File
        //peek 1: Run
        // peek 2: checking for Run merge before going to next Lumi
        // peek3: Lumi merge check
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kGoToNext, 3));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        REQUIRE(counter == 2U);
      }
      SECTION("peek for Event") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kGoToNext, 4));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{1}, counter));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addBeginAction(
                std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
            eventScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2}, counter));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{3}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        REQUIRE(counter == 4U);
      }

      SECTION("read for Run") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kRead, 0));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
      }
      SECTION("read for Lumi") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kRead, 1));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        REQUIRE(counter == 2U);
      }
      SECTION("read for Event") {
        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kRead, 2));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{1}, counter));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addBeginAction(
                std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
            eventScheduler.addEndAction(
                std::make_unique<CheckTransitionOrderAction>(kEventKey, std::vector<unsigned int>{}, counter));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{2}, counter));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{3}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        REQUIRE(counter == 4U);
      }

      //
      SECTION("merge for Run") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                  kLumiKey,
                                  edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
            edm::SourcePeekResult{
                edm::SourceNextState::DataTransition,
                kEventKey,
                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
            //Tests that new Run triggers supporter end Lumi and Run in correct order
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                  kLumiKey,
                                  edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)},
            edm::SourcePeekResult{
                edm::SourceNextState::DataTransition,
                kEventKey,
                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)},
            //Tests that ending job triggers supporter end Lumi and Run in correct order
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };

        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kMerge, 0));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
      }
      SECTION("merge for Lumi") {
        std::vector<edm::SourcePeekResult> transitions{
            edm::SourcePeekResult{edm::SourceNextState::File},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(1U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                  kLumiKey,
                                  edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                  kLumiKey,
                                  edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U)},
            edm::SourcePeekResult{
                edm::SourceNextState::DataTransition,
                kEventKey,
                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(1U), 1U), 1U)},
            //Tests that new Run triggers supporter end Lumi and Run in correct order
            edm::SourcePeekResult{edm::SourceNextState::DataTransition, kRunKey, edm::TransitionRecordID(2U)},
            edm::SourcePeekResult{edm::SourceNextState::DataTransition,
                                  kLumiKey,
                                  edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U)},
            edm::SourcePeekResult{
                edm::SourceNextState::DataTransition,
                kEventKey,
                edm::TransitionRecordID(edm::TransitionRecordID(edm::TransitionRecordID(2U), 1U), 1U)},
            //Tests that ending job triggers supporter end Lumi and Run in correct order
            edm::SourcePeekResult{edm::SourceNextState::Stop},
        };

        edm::SourceCoordinator coordinator(
            std::make_unique<ThrowingTestSource>(std::move(transitions), ThrowingTestSource::ThrowAt::kMerge, 0));

        unsigned int counter = 0;
        edm::ConcurrentTransitionScheduler runScheduler(kRunKey, 1);
        edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, 1);
        runScheduler.addDependentScheduler(lumiScheduler);
        edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, 1);
        lumiScheduler.addDependentScheduler(eventScheduler);

        edm::TransitionsDistributor distributor(coordinator);
        distributor.addSchedulerForTransition(kRunKey, runScheduler);
        distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
        distributor.addSchedulerForTransition(kEventKey, eventScheduler);

        runScheduler.addBeginAction(std::make_unique<PrintAction>("Run Begin"));
        runScheduler.addBeginAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{0}, counter));
        {
          lumiScheduler.addBeginAction(std::make_unique<PrintAction>("Lumi Begin"));
          lumiScheduler.addBeginAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
          {
            eventScheduler.addBeginAction(std::make_unique<PrintAction>("Event Begin"));
            eventScheduler.addEndAction(std::make_unique<PrintAction>("Event End"));
          }
          lumiScheduler.addEndAction(std::make_unique<PrintAction>("Lumi End"));
          lumiScheduler.addEndAction(
              std::make_unique<CheckTransitionOrderAction>(kLumiKey, std::vector<unsigned int>{}, counter));
        }
        runScheduler.addEndAction(std::make_unique<PrintAction>("Run End"));
        runScheduler.addEndAction(
            std::make_unique<CheckTransitionOrderAction>(kRunKey, std::vector<unsigned int>{1}, counter));

        tbb::task_arena arena(1);
        arena.execute([&distributor]() { REQUIRE_THROWS(distributor.processData()); });
        REQUIRE(counter == 2U);
      }
    }
  }
}