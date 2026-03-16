#ifndef ComponentBase_TransitionStateForConditionalAction_h
#define ComponentBase_TransitionStateForConditionalAction_h
#include "ComponentBase/TransitionStateForActionBase.h"
#include "ComponentBase/TransitionStateForReentrantAction.h"
#include "ControlFlow/AndDecisionNode.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/WaitingTask.h"

#include <memory>
#include <cassert>
namespace edm {
  class TransitionStateForConditionalAction : public TransitionStateForActionBase {
  public:
    explicit TransitionStateForConditionalAction(std::unique_ptr<TransitionStateForActionBase> iDecision,
                                           std::unique_ptr<TransitionStateForActionBase> iAction)
        : decision_(std::move(iDecision)), action_(std::move(iAction)), decisionNode_{
            std::shared_ptr<DecisionNodeBase>(decision_),
            std::shared_ptr<DecisionNodeBase>(action_)
          }
    {
      assert(decision_ and action_);
      assert(decision_->reactsToRecord() == action_->reactsToRecord());
    }
    ~TransitionStateForConditionalAction() final = default;

    // ProductConsumerBase interface
    TransitionRecordKey reactsToRecord() const final { return action_->reactsToRecord(); }
    std::vector<TransitionRecordKey> recordForProductsConsumed() const final {
      return action_->recordForProductsConsumed();
    }
    std::vector<ProductKey> productsConsumed(TransitionRecordKey const& record) const final {
      std::vector<ProductKey> products = decision_->productsConsumed(record);
      std::vector<ProductKey> actionProducts = action_->productsConsumed(record);
      products.insert(products.end(), actionProducts.begin(), actionProducts.end());
      return products;
    }
    // ProductProviderBase interface
    TransitionRecordKey recordForProductsProvided() const final { return action_->recordForProductsProvided(); }
    std::vector<ProductKey> productsProvided() const final { return action_->productsProvided(); }

    void addProviderForProducts(TransitionRecordKey iTrans,
                                ProductKey iKey,
                                TransitionProductProviderIndex iIndex) final {
      auto decisionProducts = decision_->productsConsumed(iTrans);
      if (std::find(decisionProducts.begin(), decisionProducts.end(), iKey) != decisionProducts.end()) {
        decision_->addProviderForProducts(iTrans, iKey, iIndex);
      }
      auto actionProducts = action_->productsConsumed(iTrans);
      if (std::find(actionProducts.begin(), actionProducts.end(), iKey) != actionProducts.end()) {
        action_->addProviderForProducts(iTrans, iKey, iIndex);
      }
    }

  private:
    // ProductConsumerBase interface
    void notifyProductsAvailableAsync(WaitingTaskHolder task, TransitionProcessingContext& context) final;
    void requestActionAsync(WaitingTaskHolder task, TransitionProcessingContext & context) final;
    void resetConsumerForNewTransition() final;
    // ProductProviderBase interface
    void provideProductRequestAsync(WaitingTaskHolder task, TransitionProcessingContext& context);
   
    // DecisionNodeBase interface
    void makeDecisionAsync_(WaitingTaskHolder task,
                            TransitionProcessingContext& context,
                            RequestState state) final;

    void resetAction_() final;

    std::shared_ptr<TransitionStateForActionBase> decision_;
    std::shared_ptr<TransitionStateForActionBase> action_;
    AndDecisionNode decisionNode_;
  };
}  // namespace edm

#endif