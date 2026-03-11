#ifndef ProductHandling_ProductConsumerBase_h
#define ProductHandling_ProductConsumerBase_h

#include <vector>
#include <optional>

#include "DataModel/TransitionRecordKey.h"
#include "ProductHandling/TransitionProductProviderIndex.h"

namespace edm {
  class WaitingTaskHolder;
  class TransitionContext;
  class TransitionProcessingContext;
  class TransitionRecordKey;
  class ProductKey;
  class TransitionProductProviderIndex;
  class ProductConsumerBase {
  public:
    ProductConsumerBase() = default;
    virtual ~ProductConsumerBase() = default;

    /// @brief The Record for which the consumer will do an action
    virtual TransitionRecordKey reactsToRecord() const = 0;
    /// @brief Returns true only if the action is to be performed at the end phase of the transition
    virtual bool reactsAtEnd() const { return false; }
    /// @brief Returns nullopt if the action is global, not associated to a give stream.
    /// if associated to a stream, the action will be performed during the proper stream phase for the reacts to record
    virtual std::optional<TransitionRecordKey> relatedStream() const;

    /// @brief All the records for which the consumer will consume products during the action
    virtual std::vector<TransitionRecordKey> recordForProductsConsumed() const = 0;
    /// @brief The products consumed by this consumer from a given record
    virtual std::vector<ProductKey> productsConsumed(TransitionRecordKey const&) const = 0;

    /// @brief Called by the scheduler to inform the consumer about which provider to use to get the products
    virtual void addProviderForProducts(TransitionRecordKey iTrans, ProductKey, TransitionProductProviderIndex iIndex) = 0;

    /// @brief Called by a provider to notify that data products are available
    virtual void notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext & context) = 0;

    /// @brief Called by the scheduler when the consumer's action is to be performed maybe called several times
    /// will trigger requests to all providers if products are not yet available
    /// @param task
    /// @param context
    virtual void requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext & context) = 0;

    virtual void resetConsumerForNewTransition() = 0;  
  };
}  // namespace edm

#endif