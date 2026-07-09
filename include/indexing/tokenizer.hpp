#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include "engine/types.hpp"

namespace Engine {

    class Tokenizer{
    public:
        static std::vector<TokenMatch> tokenize(std::string_view file_content) {
            std::vector<TokenMatch> tokens;

            size_t cursor = 0;
            size_t current_line = 1;
            size_t length = file_content.length();
            
            while(cursor < length) {
                
                while(cursor < length && !isalnum(file_content[cursor]) && file_content[cursor] != '_') {
                    if(file_content[cursor] == '\n') current_line++;
                    cursor++;
                }

                if(cursor == length) break;

                size_t token_start = cursor;

                while(cursor < length && (isalnum(file_content[cursor]) || file_content[cursor] == '_')) {
                    cursor ++;
                }

                if(cursor > token_start) {
                    std::string clean_token;
                    clean_token.reserve(cursor - token_start);

                    for (size_t i = token_start; i < cursor; ++i) {
                        clean_token.push_back(std::tolower(static_cast<unsigned char>(file_content[i])));
                    }

                    if(clean_token.size() > 2 && STOP_WORDS.find(clean_token) == STOP_WORDS.end()) {
                        tokens.push_back({clean_token, current_line});
                    }
                }
            }
            
            return tokens;
        }
    private:
        inline static const std::unordered_set<std::string> STOP_WORDS = {
            // 1. Primitives & Variable Declarations
            "int", "void", "char", "bool", "float", "double", "unsigned", "long", "short", 
            "size_t", "auto", "string", "let", "var",
            
            // 2. OOP, Classes & Types
            "class", "struct", "interface", "extends", "implements", "public", "private", 
            "protected", "virtual", "override", "abstract", "final", "this", "friend", "type",
            
            // 3. Control Flow & Logic
            "return", "true", "false", "none", "null", "nullptr", "undefined", 
            "while", "break", "continue", "switch", "case", "elif", "pass", "yield", "with",
            
            // 4. Functions & Scoping
            "def", "function", "lambda", "global", "nonlocal",
            
            // 5. Async & Exceptions
            "async", "await", "throw", "catch", "try", "except", "finally",
            
            // 6. Memory, Operators & Modifiers
            "new", "delete", "sizeof", "const", "static", "inline", "explicit", "mutable", 
            "constexpr", "volatile", "typeof", "instanceof", "synchronized", "transient",
            
            // 7. Architecture, Modules & Preprocessor
            "import", "export", "from", "package", "namespace", "using", "template", "typename", 
            "operator", "include", "pragma", "once", "define", "ifndef", "endif",
            
            // 8. High-Frequency Standard Library / Built-ins
            "std", "vector", "map", "cout", "endl", "print", "println", "console", "log", 
            "system", "out", "math"
        };
    };
}