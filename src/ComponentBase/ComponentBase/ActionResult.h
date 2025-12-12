#ifndef ComponentBase_ActionResult_h
#define ComponentBase_ActionResult_h
namespace edm {
  enum class ActionResultStatus {
    ACCEPT,
    REJECT,
    PENDING,
    EXCEPTION
  };

  class ActionResult {
  public:
    ActionResult() : status_{ActionResultStatus::PENDING} {}
    explicit ActionResult(ActionResultStatus status) : status_{status} {}
    ActionResultStatus status() const { return status_; }

    void setStatus(ActionResultStatus status) { status_ = status; }
    bool isAccept() const { return status_ == ActionResultStatus::ACCEPT; }
    bool isReject() const { return status_ == ActionResultStatus::REJECT; }
    bool isPending() const { return status_ == ActionResultStatus::PENDING; }
    bool isException() const { return status_ == ActionResultStatus::EXCEPTION; }
    private:
    ActionResultStatus status_;
  };
}  // namespace edm
#endif