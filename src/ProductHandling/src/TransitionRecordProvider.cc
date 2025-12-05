#include "ProductHandling/TransitionRecordProvider.h"
#include "ProductHandling/ProductsProvider.h"
#include "ProductHandling/TransitionRecordImpl.h"
#include "ProductHandling/ProductTransitionRecordIndexHelper.h"

edm::TransitionRecordProvider::TransitionRecordProvider(TransitionRecordKey key, std::shared_ptr<ProductTransitionRecordIndexHelper> iHelper, unsigned int allowedConcurrency):
    key_(std::move(key)), helper_(iHelper), allowedConcurrency_(allowedConcurrency) {
}

void edm::TransitionRecordProvider::addResolversFrom(ProductsProvider& provider){
    
}
