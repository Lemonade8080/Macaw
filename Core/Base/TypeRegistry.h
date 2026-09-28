#include "TypeInfo.h"

#include <vector>

namespace TypeRegistry {
    void Register(const FTypeInfo* Type);
    const FTypeInfo* Find(std::string_view TypeName);
    std::vector<const FTypeInfo*> GetRegisteredTypes();
}
