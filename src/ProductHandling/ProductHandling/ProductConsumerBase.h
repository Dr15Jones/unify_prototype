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

    void addProviderForProducts(TransitionRecordKey iTrans, TransitionProductProviderIndex iIndex);

    /// @brief Called by a provider to notify that data products are available
    void notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context);

    /// @brief Called by the scheduler when the consumer's action is to be performed maybe called several times
    /// will trigger requests to all providers if products are not yet available
    /// @param task
    /// @param context
    void requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext const & context);

    void resetConsumerForNewTransition();
  
  protected:
    bool areAllProductsAvaliable() const { return providersDoneCount.load() == providers_.size(); }

  private:
    virtual void resetConsumer_() {};
    /// @brief Called when data is available from all providers and requestActionAsync has been called
    /// @param task
    /// @param context
    virtual void reactToAllProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext const & context) = 0;
    std::vector<std::pair<TransitionRecordKey, TransitionProductProviderIndex>> providers_;
    std::atomic<size_t> providersDoneCount{0};
    std::atomic<bool> haveRequestedProducts_{false};
  };
}  // namespace edm

#endif