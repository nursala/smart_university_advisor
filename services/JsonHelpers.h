#pragma once

#include <json/json.h>

// Builds a Json::Value array by applying `toJson` to each element of
// `container`, in order. Replaces the repeated
// "Json::Value array(Json::arrayValue); for (...) { array.append(...); }"
// loop pattern used across the service layer.
template <typename Container, typename F>
Json::Value toJsonArray(const Container &container, F &&toJson)
{
    Json::Value array(Json::arrayValue);
    for (const auto &item : container)
    {
        array.append(toJson(item));
    }
    return array;
}
