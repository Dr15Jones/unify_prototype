#include "ProductHandling/ProductConsumerBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "DataModel/TransitionRecordKey.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "ProductHandling/TransitionProviderContext.h"
#include "ProductHandling/TransitionProductProviders.h"
namespace edm {

  std::optional<TransitionRecordKey> ProductConsumerBase::relatedStream() const { return std::nullopt; }

}  // namespace edm