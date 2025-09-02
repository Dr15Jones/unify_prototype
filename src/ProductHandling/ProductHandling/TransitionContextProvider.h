#ifndef ProductHandling_TransitionContextProvider_h
#define ProductHandling_TransitionContextProvider_h
/*
Handles assembling a TransitionContext for a given active transition. This makes use of the dependencies between TransitionRecords to only provide the correct records.
*/

namespace edm {

  class TransitionContextProvider {
  public:
    TransitionContextProvider() = default;
  };
}  // namespace edm
#endif