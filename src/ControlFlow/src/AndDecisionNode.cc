#include "ControlFlow/AndDecisionNode.h"
namespace edm {
    ControlFlowStatus AndDecisionNode::fullDecisionLogic_(ControlFlowStatus left, ControlFlowStatus right) const {
      if (left == ControlFlowStatus::EXCEPTION || right == ControlFlowStatus::EXCEPTION) {
        return ControlFlowStatus::EXCEPTION;
      }
      if (left == ControlFlowStatus::GO && right == ControlFlowStatus::GO) {
        return ControlFlowStatus::GO;
      }
      return ControlFlowStatus::SKIP;
    }
    ControlFlowStatus AndDecisionNode::shortCircuitLogic_(ControlFlowStatus decision) const {
      if (decision == ControlFlowStatus::EXCEPTION) {
        return ControlFlowStatus::EXCEPTION;
      }
      if (decision == ControlFlowStatus::SKIP) {
        return ControlFlowStatus::SKIP;
      }
      return ControlFlowStatus::NOT_STARTED;
    }
}