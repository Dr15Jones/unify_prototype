#ifndef ProducHandling_ProvidersKey_h
#define ProducHandling_ProvidersKey_h

#include <string>

namespace edm {
  class ProvidersKey {
  public:
  template<typename T>
  requires std::convertible_to<T, std::string>
     explicit ProvidersKey(T&& iName) : name_(std::move(iName)) {}
    std::string const& name() const noexcept { return name_; }

    auto operator<=>(const ProvidersKey&) const = default;
  private:
    std::string name_;
  };
}  // namespace edm

#endif