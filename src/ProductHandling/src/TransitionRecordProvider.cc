#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductResolversProvider.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/ProductResolverIndexHelper.h"

edm::TransitionRecordProvider::TransitionRecordProvider(TransitionRecordKey key, std::shared_ptr<ProductResolverIndexHelper> iHelper, unsigned int allowedConcurrency):
    key_(std::move(key)), helper_(iHelper), allowedConcurrency_(allowedConcurrency) {
}

void edm::TransitionRecordProvider::addResolversFrom(ProductResolversProvider& provider){
    
}
