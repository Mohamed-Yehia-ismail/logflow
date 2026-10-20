#pragma once

#include <string>
#include <typeindex>

#include "logflow/model/LogRecord.hpp"
#include "logflow/model/ProjectedRecord.hpp"

namespace logflow {

// The item type flowing between two components, with a readable name for messages.
struct ItemType {
    std::type_index type;
    const char* name;

    template <typename T>
    static ItemType of();

    bool operator==(const ItemType& other) const { return type == other.type; }
    bool operator!=(const ItemType& other) const { return type != other.type; }
};

template <typename T>
struct ItemTypeName;  // defined for every type that may flow through a configured pipeline

template <>
struct ItemTypeName<std::string> {
    static constexpr const char* value = "text lines";
};
template <>
struct ItemTypeName<LogRecord> {
    static constexpr const char* value = "parsed records";
};
template <>
struct ItemTypeName<ProjectedRecord> {
    static constexpr const char* value = "projected records";
};

template <typename T>
ItemType ItemType::of() {
    return ItemType{std::type_index(typeid(T)), ItemTypeName<T>::value};
}

}  // namespace logflow
