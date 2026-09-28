#pragma once

#include <string>

namespace Engine {
    inline std::string escape_like_literal(const std::string& value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (char character : value) {
            if (character == '%' || character == '_' || character == '\\') {
                escaped += '\\';
            }
            escaped += character;
        }
        return escaped;
    }
}
