#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "oneapi/tbb/global_control.h"

#include "Concurrency/WaitingTask.h"
#include "Concurrency/FinalWaitingTask.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "DataModel/TransitionContext.h"
#include "ControlFlow/StartDecisionGraph.h"
#include "ControlFlow/AndDecisionNode.h"
#include "ControlFlow/OrDecisionNode.h"
#include "DummyDecisionNode.h"
#include "SpyingDecisionNode.h"

TEST_CASE("ControlFlow Decision Nodes", "[ControlFlow]") {
  oneapi::tbb::global_control control(oneapi::tbb::global_control::max_allowed_parallelism, 1);

  SECTION("Dummy Decision Node") {
    SECTION("GO Status") {
      edm::TransitionContext context;
      auto node = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::GO);
      edm::SpyingDecisionNode spyingNode{node};
    
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};


      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::GO);
    }
    SECTION("SKIP Status") {
      edm::TransitionContext context;
      auto node = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::SKIP);
      edm::SpyingDecisionNode spyingNode{node};
    
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};


      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::SKIP);
    }
    SECTION("EXCEPTION Status") {
      edm::TransitionContext context;
      auto node = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::EXCEPTION);
      edm::SpyingDecisionNode spyingNode{node};
    
      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};


      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::EXCEPTION);
    }
  }
  SECTION("AND Decision Node") {
    SECTION("GO and SKIP Status") {
      edm::TransitionContext context;
      std::shared_ptr<edm::DecisionNodeBase> node1 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::GO);
      std::shared_ptr<edm::DecisionNodeBase> node2 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::SKIP);
      auto andNode = std::make_shared<edm::AndDecisionNode >(node1, node2);
      edm::SpyingDecisionNode spyingNode{andNode};

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::SKIP);
    }
    SECTION("GO and GO Status") {
      edm::TransitionContext context;
      std::shared_ptr<edm::DecisionNodeBase> node1 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::GO);
      std::shared_ptr<edm::DecisionNodeBase> node2 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::GO);
      auto andNode = std::make_shared<edm::AndDecisionNode >(node1, node2);
      edm::SpyingDecisionNode spyingNode{andNode};

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::GO);
    }
    SECTION("SKIP and GO Status") {
      edm::TransitionContext context;
      std::shared_ptr<edm::DecisionNodeBase> node1 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::SKIP);
      std::shared_ptr<edm::DecisionNodeBase> node2 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::GO);
      auto andNode = std::make_shared<edm::AndDecisionNode >(node1, node2);
      edm::SpyingDecisionNode spyingNode{andNode};

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::SKIP);
    }
    SECTION("SKIP and SKIP Status") {
      edm::TransitionContext context;
      std::shared_ptr<edm::DecisionNodeBase> node1 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::SKIP);
      std::shared_ptr<edm::DecisionNodeBase> node2 = std::make_shared<edm::DummyDecisionNode >(edm::ControlFlowStatus::SKIP);
      auto andNode = std::make_shared<edm::AndDecisionNode >(node1, node2);
      edm::SpyingDecisionNode spyingNode{andNode};

      oneapi::tbb::task_group group;
      edm::FinalWaitingTask waitTask{group};

      edm::StartDecisionGraph startNode;
      startNode.addLeafNode(&spyingNode);
      startNode.startAsync(edm::WaitingTaskHolder(group, &waitTask), context);
      waitTask.waitNoThrow();
      REQUIRE(spyingNode.status() == edm::ControlFlowStatus::SKIP);
    }

  }
}