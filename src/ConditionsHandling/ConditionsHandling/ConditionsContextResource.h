#ifndef ConditionsHandling_ConditionsContextResource_h
#define ConditionsHandling_ConditionsContextResource_h

#include <vector>
#include <memory>

#include "ConditionsHandling/ConditionsRecordResource.h"

namespace edm {
  class ConditionsContextResource {
  public:
    ConditionsContextResource() = default;
    ~ConditionsContextResource() = default;

    void addRecordResource(std::shared_ptr<ConditionsRecordResource> recordResource) {
      recordResources_.push_back(recordResource);
    }

    const std::vector<std::shared_ptr<ConditionsRecordResource>>& recordResources() const {
      return recordResources_;
    }

    void clear() { recordResources_.clear(); }

  private:
    std::vector<std::shared_ptr<ConditionsRecordResource>> recordResources_;
  };
}  // namespace edm

#endif  // ConditionsHandling_ConditionsContextResource_h