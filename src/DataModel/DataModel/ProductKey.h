#ifndef DataModel_ProductKey_h
#define DataModel_ProductKey_h

/*

*/

#include <string>
#include <compare>
#include "Base/TypeIDBase.h"

namespace edm {
  class ProductKey {
  public:
    ProductKey() = default;
    ProductKey(TypeIDBase typeID, std::string moduleLabel, std::string productInstanceName, std::string processName)
        : typeID_(typeID),
          moduleLabel_(std::move(moduleLabel)),
          productInstanceName_(std::move(productInstanceName)),
          processName_(std::move(processName)) {}
    TypeIDBase const& typeID() const { return typeID_; }
    std::string const& moduleLabel() const { return moduleLabel_; }
    std::string const& productInstanceName() const { return productInstanceName_; }
    std::string const& processName() const { return processName_; }

    std::strong_ordering operator<=>(ProductKey const& other) const noexcept = default;

    template <typename T>
    static ProductKey makeKey(std::string moduleLabel ,
                              std::string productInstanceName,
                              std::string processName) { 
      return ProductKey(TypeIDBase{typeid(T)}, std::move(moduleLabel), std::move(productInstanceName), std::move(processName));
    }
  private:
    TypeIDBase typeID_;
    std::string moduleLabel_;
    std::string productInstanceName_;
    std::string processName_;
  };
}  // namespace edm

#endif