#include "tokenizer.hpp"
#include <cctype>
#include <algorithm>
using namespace std;

namespace Engine {
    vector<TokenMatch> Tokenizer::tokenize(string_view file_content) {

        vector<TokenMatch> tokens;

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
                string_view raw_token = file_content.substr(token_start, cursor - token_start);
                string clean_token = normalize(raw_token);

                if(!clean_token.empty()) tokens.push_back({clean_token, current_line});
            }
        }
        
        return tokens;
    }

    string Tokenizer::normalize(string_view raw_token) {

        string clean_str(raw_token);
        
        transform(clean_str.begin(), clean_str.end(), clean_str.begin(), [](unsigned char c) {
            return tolower(c);
        });
        
        return clean_str;
    }
}