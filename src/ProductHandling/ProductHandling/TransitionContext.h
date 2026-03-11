#ifndef ProductHandling_TransitionContext_h
#define ProductHandling_TransitionContext_h
/* 
Holds the TransitionRecordImpl related to the present transition.
*/

#include "ProductHandling/TransitionRecordImpl.h"
#include "DataModel/TransitionRecordKey.h"
#include <map>
namespace edm {
  class TransitionContext {
    public:
    TransitionContext() = default;

    void insert(TransitionRecordImpl& record);

    TransitionRecordImpl const* get(TransitionRecordKey const& key) const;

    TransitionRecordImpl* get(TransitionRecordKey const& key);

  private:
    std::map<TransitionRecordKey, TransitionRecordImpl*> records_;
  };
}  // namespace edm

#endif