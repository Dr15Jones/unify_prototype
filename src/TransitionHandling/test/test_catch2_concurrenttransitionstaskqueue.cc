#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <oneapi/tbb/task_arena.h>

#include <catch2/catch_all.hpp>
#include "TransitionHandling/ConcurrentTransitionsTaskQueue.h"

//NOTE: the design of ConcurrentTransitionScheduler is such that when a pushAndPause is called
// no other task will ever be put into the queue until after the queued task has executed.
// To simulate that behavior this test serializes the pushAndPause calls by only calling the next pushAndPause after the previous one has executed.

TEST_CASE("ConcurrentTransitionsTaskQueue behavior") {
  SECTION("reports configured concurrency limit") {
    edm::ConcurrentTransitionsTaskQueue queue(3);
    REQUIRE(queue.concurrencyLimit() == 3);
  }

  SECTION("pushAndPause") {
    SECTION("concurrency 1") {
      tbb::task_arena arena(1);
      edm::ConcurrentTransitionsTaskQueue queue(1);

      std::atomic<int> started{0};
      std::atomic<int> finished{0};

      arena.execute([&queue, &started, &finished]() mutable {
        oneapi::tbb::task_group group;
        queue.pushAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(0 == started++);
          REQUIRE(0 == finished++);
          resumer.resume();

          queue.pushAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
            REQUIRE(1 == started++);
            REQUIRE(1 == finished++);
            resumer.resume();
          });
        });
        group.wait();
      });
      REQUIRE(finished.load() == 2);
    }
  }

  SECTION("pushToAllAndPause") {
    SECTION("concurrency 1") {
      edm::ConcurrentTransitionsTaskQueue queue(1);

      std::atomic<int> started{0};
      std::atomic<int> finished{0};
      tbb::task_arena arena(1);
      arena.execute([&queue, &started, &finished]() mutable {
        oneapi::tbb::task_group group;

        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(0 == started++);
          REQUIRE(0 == finished++);
          resumer.resume();
        });

        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(1 == started++);
          REQUIRE(1 == finished++);
          resumer.resume();
        });

        group.wait();
      });
      REQUIRE(finished.load() == 2);
    }
  }

  SECTION("Event Processing approximation") {
    SECTION("queue size 1") {
      edm::ConcurrentTransitionsTaskQueue queue(1);

      std::atomic<int> started{0};
      std::atomic<int> finished{0};

      tbb::task_arena arena(1);
      arena.execute([&queue, &started, &finished]() mutable {
        oneapi::tbb::task_group group;
        //stream begin Run
        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(started++ == 0);
          REQUIRE(finished++ == 0);
          resumer.resume();
        });
        //stream begin Lumi
        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(started++ == 1);
          REQUIRE(finished++ == 1);
          resumer.resume();
        });

        //event
        queue.pushAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
          REQUIRE(started++ == 2);
          REQUIRE(finished++ == 2);
          resumer.resume();

          //stream end Lumi
          queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
            REQUIRE(started++ == 3);
            REQUIRE(finished++ == 3);
            resumer.resume();
          });
          //stream end Run
          queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t) {
            REQUIRE(started++ == 4);
            REQUIRE(finished++ == 4);
            resumer.resume();
          });
        });

        group.wait();
      });
      REQUIRE(finished.load() == 5);
    }
    SECTION("queue size 2") {
      edm::ConcurrentTransitionsTaskQueue queue(2);

      std::array<std::atomic<int>, 2> started{};
      std::atomic<int> eventStream{-1};

      tbb::task_arena arena(1);
      arena.execute([&queue, &started, &eventStream]() mutable {
        oneapi::tbb::task_group group;
        //stream begin Run
        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t index) {
          REQUIRE(index >= 0);
          REQUIRE(index < 2);
          REQUIRE(started[index]++ == 0);
          resumer.resume();
        });
        //stream begin Lumi
        queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t index) {
          REQUIRE(index >= 0);
          REQUIRE(index < 2);
          REQUIRE(started[index]++ == 1);
          resumer.resume();
        });

        //event
        queue.pushAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t index) {
          REQUIRE(index >= 0);
          REQUIRE(index < 2);
          REQUIRE(eventStream == -1);
          eventStream = index;
          REQUIRE(started[index]++ == 2);
          resumer.resume();

          //stream end Lumi
          queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t index) {
            auto s = started[index]++;
            if (index == eventStream) {
              REQUIRE(s == 3);
            } else {
              REQUIRE(s == 2);
            }
            resumer.resume();
          });
          //stream end Run
          queue.pushToAllAndPause(group, [&](edm::ConcurrentTransitionsTaskQueue::Resumer resumer, std::size_t index) {
            auto s = started[index]++;
            if (index == eventStream) {
              REQUIRE(s == 4);
            } else {
              REQUIRE(s == 3);
            }
            resumer.resume();
          });
        });

        group.wait();
      });
      REQUIRE(eventStream.load() != -1);
      int nonStream = 1 - eventStream.load();
      REQUIRE(started[eventStream.load()] == 5);
      REQUIRE(started[nonStream] == 4);
    }
  }
}
