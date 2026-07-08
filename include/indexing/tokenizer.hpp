#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <algorithm>
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

                    if(clean_token.size() > 2) tokens.push_back({clean_token, current_line});
                }
            }
            
            return tokens;
        }
    };
}