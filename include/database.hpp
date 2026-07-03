#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <sqlite3.h>
#include "tokenizer.hpp"
using namespace std;

namespace Engine {
    class Database {
    public:
        Database(const string &db_path);
        ~Database();

        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        bool init();
        int insert_document(const string &file_path);
        bool insert_tokens(int document_id, const vector<TokenMatch> &tokens);
        void print_inverted_index();

    private:
        sqlite3 *db_handle = nullptr;
    };
}