#include "ProductHandling/TransitionContext.h"

void edm::TransitionContext::insert(TransitionRecordImpl& record) { records_.emplace(record.key(), &record); }

edm::TransitionRecordImpl const* edm::TransitionContext::get(TransitionRecordKey const& key) const {
  auto it = records_.find(key);
  if (it != records_.end()) {
    return it->second;
  }
  return nullptr;  // Return nullptr if the record is not found
}

edm::TransitionRecordImpl* edm::TransitionContext::get(TransitionRecordKey const& key) {
  auto it = records_.find(key);
  if (it != records_.end()) {
    return it->second;
  }
  return nullptr;  // Return nullptr if the record is not found
}
