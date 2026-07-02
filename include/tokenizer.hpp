#pragma once
#include <string>
#include <string_view>
#include <vector>
using namespace std;

namespace Engine {
    struct TokenMatch {
        string token;
        size_t line_number;
    };

    class Tokenizer{
    public:
        static vector<TokenMatch> tokenize(string_view file_content);

    private:
        static string normalize(string_view raw_token);
    };
}