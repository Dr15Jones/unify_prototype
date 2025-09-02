#ifndef DataModel_ProductGetToken_h
#define DataModel_ProductGetToken_h

#include "DataModel/ProductGetTokenIndex.h"

namespace edm {
  struct UnknownTransitionRecord {};
  template <typename T, typename TRecord = UnknownTransitionRecord>
  class ProductGetToken {
  public:
    ProductGetToken() = default;
    explicit ProductGetToken(ProductGetTokenIndex index) : m_index(index) {}
    ProductGetToken(ProductGetToken const&) = default;
    ProductGetToken& operator=(ProductGetToken const&) = default;
    ProductGetToken(ProductGetToken&&) = default;
    ProductGetToken& operator=(ProductGetToken&&) = default;

    constexpr ProductGetTokenIndex index() const noexcept { return m_index; }
    constexpr bool isUninitialized() const noexcept { return m_index.isUninitialized(); }

  private:
    ProductGetTokenIndex m_index = ProductGetTokenIndex();
  };
}  // namespace edm
#endif