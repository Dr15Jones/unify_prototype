#include "ControlFlow/OrDecisionNode.h"
namespace edm {
    ControlFlowStatus OrDecisionNode::fullDecisionLogic_(ControlFlowStatus left, ControlFlowStatus right) const {
      if (left == ControlFlowStatus::EXCEPTION || right == ControlFlowStatus::EXCEPTION) {
        return ControlFlowStatus::EXCEPTION;
      }
      if (left == ControlFlowStatus::GO || right == ControlFlowStatus::GO) {
        return ControlFlowStatus::GO;
      }
      return ControlFlowStatus::SKIP;
    }
    ControlFlowStatus OrDecisionNode::shortCircuitLogic_(ControlFlowStatus decision) const {
      if (decision == ControlFlowStatus::EXCEPTION) {
        return ControlFlowStatus::EXCEPTION;
      }
      if (decision == ControlFlowStatus::GO) {
        return ControlFlowStatus::GO;
      }
      return ControlFlowStatus::NOT_STARTED;
    }
}