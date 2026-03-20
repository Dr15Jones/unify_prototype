#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductProviderBundle.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/TransitionRecordProductIndexHelper.h"

edm::TransitionRecordProvider::TransitionRecordProvider(TransitionRecordKey key, std::shared_ptr<TransitionRecordProductIndexHelper> iHelper, unsigned int allowedConcurrency):
    key_(std::move(key)), helper_(iHelper), allowedConcurrency_(allowedConcurrency) {
}
