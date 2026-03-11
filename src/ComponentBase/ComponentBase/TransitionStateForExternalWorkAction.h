#ifndef ComponentBase_TransitionStateForExternalWorkAction_h
#define ComponentBase_TransitionStateForExternalWorkAction_h
#include "ComponentBase/TransitionStateForActionSingleBase.h"
#include "Concurrency/WaitingTaskHolder.h"
#include "Concurrency/WaitingTask.h"

namespace edm {
  template <typename T>
  class TransitionStateForExternalWorkAction : public TransitionStateForActionSingleBase {
  public:
    explicit TransitionStateForExternalWorkAction(T&& iAction) : action_(std::forward<T>(iAction)) {}
    ~TransitionStateForExternalWorkAction() override = default;

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
        auto postTask = edm::make_waiting_task([this, &context, task](std::exception_ptr const* eptr) {
          ActionResult result;
          if (eptr and *eptr) {
            result.setStatus(ActionResultStatus::EXCEPTION);
          } else {
            try {
              result = action_.work(context.transitionContext());
            } catch (...) {
              result.setStatus(ActionResultStatus::EXCEPTION);
            }
          }
          doneWorkAsync(std::move(task), context, result);
        });

        try {
          action_.acquire(context.transitionContext(), WaitingTaskHolder(*task.group(), postTask));
        } catch (...) {
          edm::ActionResult result(ActionResultStatus::EXCEPTION);
          WaitingTaskHolder localTask(task);
          localTask.doneWaiting(std::current_exception());
          doneWorkAsync(std::move(localTask), context, result);
          return;
        }
      });
    }

  private:
    T action_;
  };
}  // namespace edm

#endif