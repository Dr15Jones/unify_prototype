#include "DataModel/TransitionRecordKey.h"
#include "Base/TypeDemangler.h"

std::string edm::TransitionRecordKey::name() const { return edm::typeDemangle(typeID_.name()); }