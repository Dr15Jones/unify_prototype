#ifndef ComponentBase_TransitionStateForReentrantAction_h
#define ComponentBase_TransitionStateForReentrantAction_h
#include "ComponentBase/TransitionStateForActionSingleBase.h"
#include "ProductHandling/TransitionProcessingContext.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/WaitingTask.h"

namespace edm {
  template <typename T>
  class TransitionStateForReentrantAction : public TransitionStateForActionSingleBase {
  public:
    template<typename... Args>
    explicit TransitionStateForReentrantAction(Args&&... iArgs) : action_(std::forward<Args>(iArgs)...) {}
    ~TransitionStateForReentrantAction() override = default;

    // ProductConsumerBase interface
    TransitionRecordKey reactsToRecord() const override { return action_.reactsToRecord(); }
    std::vector<TransitionRecordKey> recordForProductsConsumed() const override {
      return action_.recordForProductsConsumed();
    }
    std::vector<ProductKey> productsConsumed(TransitionRecordKey const& record) const override {
      return action_.productsConsumed(record);
    }
    // ProductProviderBase interface
    TransitionRecordKey recordForProductsProvided() const override { return action_.recordForProductsProvided(); }
    std::vector<ProductKey> productsProvided() const override { return action_.productsProvided(); }

  protected:
    /// @brief The actual work to be done by the action. Once the work is done
    /// the derived class must call doneWorkAsync to notify completion
    void workAsync(WaitingTaskHolder task, TransitionProcessingContext& context) final {
      task.group()->run([this, task = std::move(task), &context]() {
        ActionResult result;
        try {
          result = action_.work(context.transitionContext());
        } catch (...) {
          result.setStatus(ActionResultStatus::EXCEPTION);
          WaitingTaskHolder localTask(task);
          localTask.doneWaiting(std::current_exception());
          doneWorkAsync(std::move(localTask), context, result);
          return;
        }
        doneWorkAsync(std::move(task), context, result);
      });
    }

  private:
    T action_;
  };
}  // namespace edm

#endif