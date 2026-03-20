#ifndef ProductHandling_TransitionRecordProvider_h
#define ProductHandling_TransitionRecordProvider_h

/* Handles providing TransitionRecordImpl objects for a given transition. Used by TransitionContextProvider to assemble a TransitionContext.
* NOTE: in the EventSetup system, the RecordProvider also handles the IOV behavior. Could try to move that to a separate component here.
*/

#include "ProductHandling/TransitionRecordImpl.h"
#include "DataModel/TransitionRecordKey.h"
#include <memory>
#include <vector>

namespace edm {
  class ProductProviderBundle;
  class TransitionRecordProvider {
  public:
    TransitionRecordProvider() = delete;
    explicit TransitionRecordProvider(TransitionRecordKey key,
                                      std::shared_ptr<TransitionRecordProductIndexHelper>,
                                      unsigned int allowedConcurrency);

    TransitionRecordKey const& key() const { return key_; }
  private:
    std::shared_ptr<TransitionRecordProductIndexHelper> helper_;
    std::vector<std::unique_ptr<TransitionRecordImpl>> records_;
    TransitionRecordKey key_;
    unsigned int allowedConcurrency_;
    ;
  };
}  // namespace edm

#endif