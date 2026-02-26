#ifndef ProductHandling_TransitionProcessingContext_h
#define ProductHandling_TransitionProcessingContext_h

namespace edm {
  class TransitionContext;
  class TransitionProviderContext;
  class TransitionProcessingContext {
  public:
    TransitionProcessingContext(TransitionContext& transitionContext, TransitionProviderContext const& providerContext)
        : transitionContext_{transitionContext}, providerContext_{providerContext} {}

    TransitionContext const& transitionContext() const { return transitionContext_; }
    TransitionContext& transitionContext() { return transitionContext_; }
    TransitionProviderContext const& providerContext() const { return providerContext_; }

  private:
    TransitionContext& transitionContext_;
    TransitionProviderContext const& providerContext_;
  };
}  // namespace edm
#endif