#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <numeric>
#include <format>

#include <oneapi/tbb/task_arena.h>

#include "TransitionHandling/AsyncActionBase.h"
#include "TransitionHandling/ConcurrentTransitionID.h"
#include "TransitionHandling/ConcurrentTransitionScheduler.h"
#include "TransitionHandling/SourceBase.h"
#include "TransitionHandling/SourceCoordinator.h"
#include "DataModel/TransitionRecordID.h"
#include "TransitionHandling/TransitionsDistributor.h"

namespace {
  struct Run {};
  constexpr auto kRunKey = edm::TransitionRecordKey::makeKey<Run>();

  struct Lumi {};
  constexpr auto kLumiKey = edm::TransitionRecordKey::makeKey<Lumi>();

  struct Event {};
  constexpr auto kEventKey = edm::TransitionRecordKey::makeKey<Event>();

  struct AtomicContainer {
    std::unique_ptr<std::atomic<std::size_t>[]> data_;
    std::size_t size_;
    AtomicContainer(std::size_t size) : data_(std::make_unique<std::atomic<std::size_t>[]>(size)), size_(size) {
    }
    using iterator = std::atomic<std::size_t>*;

    std::atomic<std::size_t>& operator[](std::size_t index) {
      return data_[index];
    }
    iterator begin() { return data_.get(); }
    iterator end() { return data_.get() + size_; }

    std::size_t size() const { return size_; }
    std::size_t total() const {
      return std::accumulate(data_.get(), data_.get() + size_, std::size_t{0},
                             [](std::size_t sum, const std::atomic<std::size_t>& v) { return sum + v.load(); });
    }
  };
  struct Config {
    std::size_t threads = 0;
    std::size_t runConcurrency = 0;
    std::size_t lumiConcurrency = 0;
    std::size_t eventConcurrency = 0;
    std::size_t runs = 0;
    std::size_t lumisPerRun = 0;
    std::size_t eventsPerLumi = 0;
  };

  void usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0
        << " --threads N"
        << " --run-concurrency N"
        << " --lumi-concurrency N"
        << " --event-concurrency N"
        << " --runs N"
        << " --lumis-per-run N"
        << " --events-per-lumi N\n";
  }

  std::size_t parsePositive(const std::string& text, const char* key) {
    std::size_t pos = 0;
    unsigned long long v = 0;
    try {
      v = std::stoull(text, &pos);
    } catch (std::exception const&) {
      throw std::runtime_error(std::string("Invalid value for ") + key + ": '" + text + "'");
    }
    if (pos != text.size() || v == 0) {
      throw std::runtime_error(std::string("Value for ") + key + " must be a positive integer");
    }
    return static_cast<std::size_t>(v);
  }

  Config parseArgs(int argc, char** argv) {
    std::unordered_map<std::string, std::string> values;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--", 0) != 0) {
        throw std::runtime_error("Arguments must be named (start with --)");
      }

      auto eq = arg.find('=');
      if (eq != std::string::npos) {
        values[arg.substr(2, eq - 2)] = arg.substr(eq + 1);
        continue;
      }

      if (i + 1 >= argc) {
        throw std::runtime_error("Missing value for argument: " + arg);
      }
      values[arg.substr(2)] = argv[++i];
    }

    auto require = [&](const char* key) -> std::size_t {
      auto found = values.find(key);
      if (found == values.end()) {
        throw std::runtime_error(std::string("Missing required argument --") + key);
      }
      return parsePositive(found->second, key);
    };

    Config cfg;
    cfg.threads = require("threads");
    cfg.runConcurrency = require("run-concurrency");
    cfg.lumiConcurrency = require("lumi-concurrency");
    cfg.eventConcurrency = require("event-concurrency");
    cfg.runs = require("runs");
    cfg.lumisPerRun = require("lumis-per-run");
    cfg.eventsPerLumi = require("events-per-lumi");
    return cfg;
  }

  class GeneratedSource final : public edm::SourceBase {
  public:
    explicit GeneratedSource(std::vector<edm::SourcePeekResult> transitions)
        : transitions_(std::move(transitions)), current_(-1) {}

    void readTransition(edm::TransitionRecordKey transitionKey, edm::ConcurrentTransitionID) final {
      validateCurrent(transitionKey);
    }

    void mergeTransition(edm::TransitionRecordKey transitionKey,
                         edm::TransitionRecordID const& recordID,
                         edm::ConcurrentTransitionID) final {
      validateCurrent(transitionKey);
      auto const& current = transitions_[current_];
      if (!current.recordID() || *current.recordID() != recordID) {
        throw std::runtime_error("mergeTransition called with unexpected record ID");
      }
    }

    edm::SourcePeekResult goToNextTransition() final {
      ++current_;
      if (current_ < static_cast<int>(transitions_.size())) {
        return transitions_[current_];
      }
      return edm::SourcePeekResult{edm::SourceNextState::Stop};
    }

  private:
    void validateCurrent(edm::TransitionRecordKey transitionKey) const {
      if (current_ < 0 || current_ >= static_cast<int>(transitions_.size())) {
        throw std::runtime_error("Source called without current transition");
      }
      auto const& current = transitions_[current_];
      if (current.state() != edm::SourceNextState::DataTransition || !current.recordKey() ||
          *current.recordKey() != transitionKey) {
        throw std::runtime_error("Source called with unexpected transition key");
      }
    }

    std::vector<edm::SourcePeekResult> transitions_;
    int current_;
  };

  class CountingAction final : public edm::AsyncActionBase {
  public:
    explicit CountingAction(AtomicContainer& count) : count_(count) {}

    void performAsync(edm::WaitingTaskHolder holder,
                      edm::TransitionRecordKey const&,
                      edm::ConcurrentTransitionID id,
                      edm::TransitionRecordID const&) final {
      count_[id.id()].fetch_add(1, std::memory_order_relaxed);
      holder.doneWaiting(std::exception_ptr{});
    }

  private:
    AtomicContainer& count_;
  };

  std::vector<edm::SourcePeekResult> makeTransitions(const Config& cfg) {
    std::vector<edm::SourcePeekResult> transitions;
    transitions.reserve(1 + cfg.runs + cfg.runs * cfg.lumisPerRun + cfg.runs * cfg.lumisPerRun * cfg.eventsPerLumi + 1);

    transitions.emplace_back(edm::SourceNextState::File);
    for (std::size_t run = 1; run <= cfg.runs; ++run) {
      auto runID = edm::TransitionRecordID(static_cast<unsigned int>(run));
      transitions.emplace_back(edm::SourceNextState::DataTransition, kRunKey, runID);

      for (std::size_t lumi = 1; lumi <= cfg.lumisPerRun; ++lumi) {
        auto lumiID = edm::TransitionRecordID(runID, static_cast<unsigned int>(lumi));
        transitions.emplace_back(edm::SourceNextState::DataTransition, kLumiKey, lumiID);

        for (std::size_t event = 1; event <= cfg.eventsPerLumi; ++event) {
          auto eventID = edm::TransitionRecordID(lumiID, static_cast<unsigned int>(event));
          transitions.emplace_back(edm::SourceNextState::DataTransition, kEventKey, eventID);
        }
      }
    }
    transitions.emplace_back(edm::SourceNextState::Stop);
    return transitions;
  }
}

int main(int argc, char** argv) {
  Config cfg;
  try {
    cfg = parseArgs(argc, argv);
  } catch (std::exception const& ex) {
    std::cerr << "Argument error: " << ex.what() << "\n";
    usage(argv[0]);
    return 2;
  }

  AtomicContainer runCount{cfg.runConcurrency};
  AtomicContainer lumiCount{cfg.lumiConcurrency};
  AtomicContainer eventCount{cfg.eventConcurrency};

  try {
    auto transitions = makeTransitions(cfg);
    edm::SourceCoordinator coordinator(std::make_unique<GeneratedSource>(std::move(transitions)));

    edm::ConcurrentTransitionScheduler runScheduler(kRunKey, cfg.runConcurrency);
    runScheduler.addBeginAction(std::make_unique<CountingAction>(runCount));

    edm::ConcurrentTransitionScheduler lumiScheduler(kLumiKey, cfg.lumiConcurrency);
    lumiScheduler.addBeginAction(std::make_unique<CountingAction>(lumiCount));

    edm::ConcurrentTransitionScheduler eventScheduler(kEventKey, cfg.eventConcurrency);
    eventScheduler.addBeginAction(std::make_unique<CountingAction>(eventCount));

    runScheduler.addDependentScheduler(lumiScheduler);
    lumiScheduler.addDependentScheduler(eventScheduler);

    edm::TransitionsDistributor distributor(coordinator);
    distributor.addSchedulerForTransition(kRunKey, runScheduler);
    distributor.addSchedulerForTransition(kLumiKey, lumiScheduler);
    distributor.addSchedulerForTransition(kEventKey, eventScheduler);

    std::chrono::steady_clock::duration processingDuration{};
    tbb::task_arena arena(static_cast<int>(cfg.threads));
    arena.execute([&distributor, &processingDuration]() {
      const auto start = std::chrono::steady_clock::now();
      distributor.processData();
      const auto stop = std::chrono::steady_clock::now();
      processingDuration = stop - start;
    });

    const auto processingMs = std::chrono::duration_cast<std::chrono::milliseconds>(processingDuration).count();
    std::cout << "distributor.processData() time: " << processingMs << " ms\n";
  } catch (std::exception const& ex) {
    std::cerr << "Execution error: " << ex.what() << "\n";
    return 1;
  }

  const auto expectedRuns = cfg.runs;
  const auto expectedLumis = cfg.runs * cfg.lumisPerRun;
  const auto expectedEvents = cfg.runs * cfg.lumisPerRun * cfg.eventsPerLumi;

  const auto actualRuns = runCount.total();
  const auto actualLumis = lumiCount.total();
  const auto actualEvents = eventCount.total();

  std::cout << "Processed transitions:\n"
            << "  Runs  : " << actualRuns << " (expected " << expectedRuns << ")\n"
            << "  Lumis : " << actualLumis << " (expected " << expectedLumis << ")\n"
            << "  Events: " << actualEvents << " (expected " << expectedEvents << ")\n";

            std::cout << "Distribution details:\n";
            std::cout << "  Run concurrency distribution: \n";
            for (std::size_t i = 0; i < runCount.size(); ++i) {
              std::cout << "  ID " << i << ": " << runCount[i].load(std::memory_order_relaxed) << " runs; \n";
            }
            std::cout << "\n";
            std::cout << "  Lumi concurrency distribution: \n";
            for (std::size_t i = 0; i < lumiCount.size(); ++i) {
              std::cout << "  ID " << i << ": " << lumiCount[i].load(std::memory_order_relaxed) << " lumis; \n";
            }
            std::cout << "\n";
            std::cout << "  Event concurrency distribution: \n";
            for (std::size_t i = 0; i < eventCount.size(); ++i) {
              std::cout << "  ID " << i << ": " << eventCount[i].load(std::memory_order_relaxed) << " events; \n";
            }
            std::cout << "\n";

  const bool ok = (actualRuns == expectedRuns) && (actualLumis == expectedLumis) && (actualEvents == expectedEvents);
  if (!ok) {
    std::cerr << "ERROR: Not all required transitions were processed.\n";
    return 1;
  }

  std::cout << "SUCCESS: all required Runs, Lumis, and Events were processed.\n";
  return 0;
}
