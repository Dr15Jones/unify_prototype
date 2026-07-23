#include "ConditionsDataModel/ConditionsRecordKey.h"
#include "Base/TypeDemangler.h"

std::string edm::ConditionsRecordKey::name() const { return edm::typeDemangle(typeID_.name()); }
