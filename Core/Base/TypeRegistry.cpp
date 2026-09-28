#include "pch.h"
#include "TypeRegistry.h"

namespace {
    TMap<std::string_view, const FTypeInfo*> TypeMap{};
}

void TypeRegistry::Register(const FTypeInfo* Type) {
    if (Type == nullptr) {
        return;
    }
    TypeMap[Type->mTypeName] = Type;
}

const FTypeInfo* TypeRegistry::Find(std::string_view TypeName) {
    const auto It{TypeMap.find(TypeName)};
    if (It != TypeMap.end()) {
        return It->second;
    }
    return nullptr;
}

std::vector<const FTypeInfo*> TypeRegistry::GetRegisteredTypes() {
    std::vector<const FTypeInfo*> Types{};
    Types.reserve(TypeMap.size());

    for (const auto& [TypeName, Type] : TypeMap) {
        (void)TypeName;
        Types.push_back(Type);
    }

    std::ranges::sort(Types, [](const FTypeInfo* Left, const FTypeInfo* Right) {
        return Left->mTypeName < Right->mTypeName;
    });

    return Types;
}
