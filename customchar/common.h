#pragma once

#include <string>

namespace CC {
namespace common {

/// @brief Replace all occurrences of a substring with another substring.
/// @param s Original string.
/// @param from Replace this substring.
/// @param to With this substring.
/// @return New string with all occurrences of `from` replaced with `to`.
std::string replace(const std::string& s, const std::string& from,
                    const std::string& to);

}  // namespace common
}  // namespace CC
