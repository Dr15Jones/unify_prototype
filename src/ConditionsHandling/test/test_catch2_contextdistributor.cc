#include <memory>
#include <unordered_map>
#include <catch2/catch_all.hpp>
#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "ConditionsDataModel/ConditionsRecordKeyHash.h"
#include "ConditionsHandling/ConditionsIntervalFinder.h"
#include "ConditionsHandling/ConditionsIntervalCoordinator.h"
#include "ConditionsHandling/ValidityInterval.h"
#include "ConditionsHandling/ConditionsContextDistributor.h"
#include "ConditionsHandling/ConcurrentIntervalScheduler.h"
#include "ConditionsHandling/ConditionsContextResource.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/chain_first.h"
#include "oneapi/tbb/task_arena.h"

namespace {
  struct Alpha {};
  constexpr auto kAlphaKey = edm::ConditionsRecordKey::makeKey<Alpha>();

  struct Beta {};
  constexpr auto kBetaKey = edm::ConditionsRecordKey::makeKey<Beta>();

  struct Gamma {};
  constexpr auto kGammaKey = edm::ConditionsRecordKey::makeKey<Gamma>();

  class TestIntervalFinder final : public edm::ConditionsIntervalFinder {
  public:
    explicit TestIntervalFinder(
        std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
            intervals)
        : intervals_(std::move(intervals)) {}

    std::vector<edm::ConditionsRecordKey> findingForRecords() const final {
      std::vector<edm::ConditionsRecordKey> keys;
      for (auto const& [key, _] : intervals_) {
        keys.push_back(key);
      }
      return keys;
    }

    edm::ValidityInterval findIntervalFor(edm::ConditionsRecordKey const& key,
                                          edm::TransitionRecordID const& recordID) const final {
      auto it = intervals_.find(key);
      if (it == intervals_.end()) {
        return edm::ValidityInterval{};
      }
      for (auto const& interval : it->second) {
        if (interval.validFor(recordID)) {
          return interval;
        }
      }
      return edm::ValidityInterval{};
    }

  private:
    std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
        intervals_;
  };

}  // namespace

TEST_CASE("Test ConditionsContextDistributor", "[ConditionsContextDistributor]") {
  SECTION("no intervals") {
    std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
        intervals;

    edm::ConditionsContextDistributor distributor;
    distributor.addIntervalFinder(std::make_unique<TestIntervalFinder>(std::move(intervals)));

    auto runID1 = edm::TransitionRecordID(1U);
    tbb::task_arena arena(1);
    arena.execute([&distributor, runID1]() {
      using namespace edm::waiting_task;
      edm::ConditionsContextResource context;
      tbb::task_group group;
      edm::FinalWaitingTask finalTask(group);
      edm::WaitingTaskHolder holder(group, &finalTask);
      chain::first([&distributor, &context, runID1](edm::WaitingTaskHolder h) {
        distributor.contextForAsync(runID1, context, std::move(h));
      }) | chain::then([&context](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().empty());
      } ) | chain::then([&distributor](std::exception_ptr const* e, edm::WaitingTaskHolder h) {
        distributor.releaseCurrentResources();
        if(e) {
          h.doneWaiting(*e);
        } else {
          h.doneWaiting(std::exception_ptr{});
        }
      }) | chain::lastTask(std::move(holder));
      finalTask.wait();
    });
  }
  SECTION("single run interval with scheduler") {
    auto runID1 = edm::TransitionRecordID(1U);
    auto runID2 = edm::TransitionRecordID(2U);

    std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
        intervals;
    intervals[kAlphaKey] = {edm::ValidityInterval(runID1, runID2)};

    auto testFinder = std::make_unique<TestIntervalFinder>(std::move(intervals));

    edm::ConditionsContextDistributor distributor;
    distributor.addIntervalFinder(std::move(testFinder));
    edm::ConcurrentIntervalScheduler scheduler(kAlphaKey, 1);
    distributor.addSchedulerForRecord(kAlphaKey, &scheduler);

    tbb::task_arena arena(1);
    arena.execute([&distributor, runID1, runID2]() {
      using namespace edm::waiting_task;
      edm::ConditionsContextResource context;
      tbb::task_group group;
      edm::FinalWaitingTask finalTask(group);
      edm::WaitingTaskHolder holder(group, &finalTask);
      chain::first([&distributor, &context, runID1](edm::WaitingTaskHolder h) {
        distributor.contextForAsync(runID1, context, std::move(h));
      }) | chain::then([&context, runID1, runID2](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().size() == 1);
        REQUIRE(context.recordResources()[0]->recordKey_ == kAlphaKey);
        REQUIRE(context.recordResources()[0]->validityInterval_.begin() == runID1);
        REQUIRE(context.recordResources()[0]->validityInterval_.end() == runID2);
      }) | chain::then([&distributor](std::exception_ptr const* e, edm::WaitingTaskHolder h) {
        if(e) {
          h.doneWaiting(*e);
        } else {
          h.doneWaiting(std::exception_ptr{});
        }
        distributor.releaseCurrentResources();
      }) | chain::lastTask(std::move(holder));
      finalTask.wait();
    });

    auto available = distributor.availableRecords();
    REQUIRE(available.size() == 1);
    REQUIRE(available[0] == kAlphaKey);
  }
  SECTION("multiple intervals on same record with scheduler") {
    auto runID1 = edm::TransitionRecordID(1U);
    auto runID2 = edm::TransitionRecordID(2U);
    auto runID3 = edm::TransitionRecordID(3U);
    auto runID4 = edm::TransitionRecordID(4U);

    std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
        intervals;
    intervals[kAlphaKey] = {edm::ValidityInterval(runID1, runID2), edm::ValidityInterval(runID3, runID4)};

    auto testFinder = std::make_unique<TestIntervalFinder>(std::move(intervals));

    edm::ConditionsContextDistributor distributor;
    distributor.addIntervalFinder(std::move(testFinder));
    edm::ConcurrentIntervalScheduler scheduler(kAlphaKey, 1);
    distributor.addSchedulerForRecord(kAlphaKey, &scheduler);

    tbb::task_arena arena(1);
    arena.execute([&distributor, runID1, runID2, runID3, runID4]() {
      using namespace edm::waiting_task;
      edm::ConditionsContextResource context;
      tbb::task_group group;
      edm::FinalWaitingTask finalTask(group);
      edm::WaitingTaskHolder holder(group, &finalTask);
      chain::first([&distributor, &context, runID1](edm::WaitingTaskHolder h) {
        distributor.contextForAsync(runID1, context, std::move(h));
      }) | chain::then([&distributor, &context, runID1, runID2](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().size() == 1);
        REQUIRE(context.recordResources()[0]->recordKey_ == kAlphaKey);
        REQUIRE(context.recordResources()[0]->validityInterval_.begin() == runID1);
        REQUIRE(context.recordResources()[0]->validityInterval_.end() == runID2);
      }) | chain::then([&distributor, &context, runID3](edm::WaitingTaskHolder h) {
        context.clear();
        distributor.contextForAsync(runID3, context, std::move(h));
      }) | chain::then([&distributor, &context, runID3, runID4](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().size() == 1);
        REQUIRE(context.recordResources()[0]->recordKey_ == kAlphaKey);
        REQUIRE(context.recordResources()[0]->validityInterval_.begin() == runID3);
        REQUIRE(context.recordResources()[0]->validityInterval_.end() == runID4);
      }) | chain::then([&distributor](std::exception_ptr const* e, edm::WaitingTaskHolder h) {
        if (e) {
          h.doneWaiting(*e);
        } else {
          h.doneWaiting(std::exception_ptr{});
        }
        distributor.releaseCurrentResources();
      }) | chain::lastTask(std::move(holder));
      finalTask.wait();
    });
  }
  SECTION("multiple record types with schedulers") {
    auto runID1 = edm::TransitionRecordID(1U);
    auto runID2 = edm::TransitionRecordID(2U);
    auto lumiID1 = edm::TransitionRecordID(runID1, 1U);
    auto lumiID2 = edm::TransitionRecordID(runID1, 2U);

    std::unordered_map<edm::ConditionsRecordKey, std::vector<edm::ValidityInterval>, edm::ConditionsRecordKeyHash>
        intervals;
    intervals[kAlphaKey] = {edm::ValidityInterval(runID1, runID2)};
    intervals[kBetaKey] = {edm::ValidityInterval(lumiID1, lumiID2)};

    auto testFinder = std::make_unique<TestIntervalFinder>(std::move(intervals));

    edm::ConditionsContextDistributor distributor;
    distributor.addIntervalFinder(std::move(testFinder));
    edm::ConcurrentIntervalScheduler runScheduler(kAlphaKey, 1);
    edm::ConcurrentIntervalScheduler lumiScheduler(kBetaKey, 1);
    distributor.addSchedulerForRecord(kAlphaKey, &runScheduler);
    distributor.addSchedulerForRecord(kBetaKey, &lumiScheduler);

    tbb::task_arena arena(1);
    arena.execute([&distributor, runID1, runID2, lumiID1, lumiID2]() {
      using namespace edm::waiting_task;
      edm::ConditionsContextResource context;
      tbb::task_group group;
      edm::FinalWaitingTask finalTask(group);
      edm::WaitingTaskHolder holder(group, &finalTask);
      chain::first([&distributor, &context, runID1](edm::WaitingTaskHolder h) {
        distributor.contextForAsync(runID1, context, std::move(h));
      }) | chain::then([&context, runID1, runID2](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().size() == 1);
        REQUIRE(context.recordResources()[0]->recordKey_ == kAlphaKey);
        REQUIRE(context.recordResources()[0]->validityInterval_.begin() == runID1);
        REQUIRE(context.recordResources()[0]->validityInterval_.end() == runID2);
      }) | chain::then([&distributor, &context, lumiID1](edm::WaitingTaskHolder h) {
        context.clear();
        distributor.contextForAsync(lumiID1, context, std::move(h));
      }) | chain::then([&context, lumiID1, lumiID2, runID1, runID2](edm::WaitingTaskHolder h) {
        REQUIRE(context.recordResources().size() == 2);
        auto alphaResource = std::find_if(
            context.recordResources().begin(), context.recordResources().end(),
            [](std::shared_ptr<edm::ConditionsRecordResource> const& resource) {
              return resource->recordKey_ == kAlphaKey;
            });
        REQUIRE(alphaResource != context.recordResources().end());
        REQUIRE((*alphaResource)->validityInterval_.begin() == runID1);
        REQUIRE((*alphaResource)->validityInterval_.end() == runID2);

        auto betaResource = std::find_if(
            context.recordResources().begin(), context.recordResources().end(),
            [](std::shared_ptr<edm::ConditionsRecordResource> const& resource) {
              return resource->recordKey_ == kBetaKey;
            });
        REQUIRE(betaResource != context.recordResources().end());
        REQUIRE((*betaResource)->validityInterval_.begin() == lumiID1);
        REQUIRE((*betaResource)->validityInterval_.end() == lumiID2);
      }) | chain::then([&distributor](std::exception_ptr const* e, edm::WaitingTaskHolder h) {
          if(e) {
            h.doneWaiting(*e);
          } else {
            h.doneWaiting(std::exception_ptr{});
          }
          distributor.releaseCurrentResources();
      }) | chain::lastTask(std::move(holder));
      finalTask.wait();
    });
  }
}
