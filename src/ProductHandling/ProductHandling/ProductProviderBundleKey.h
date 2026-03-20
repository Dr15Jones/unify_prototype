#ifndef ProducHandling_ProductProviderBundleKey_h
#define ProducHandling_ProductProviderBundleKey_h

#include <string>

namespace edm {
  class ProductProviderBundleKey {
  public:
  template<typename T>
  requires std::convertible_to<T, std::string>
     explicit ProductProviderBundleKey(T&& iName) : name_(std::move(iName)) {}
    std::string const& name() const noexcept { return name_; }

    auto operator<=>(const ProductProviderBundleKey&) const = default;
  private:
    std::string name_;
  };
}  // namespace edm

#endif