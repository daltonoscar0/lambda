#include "tokenizer.hpp"
#include "tokenizer.h"   // Scalpel
#include "token.h"       // Scalpel TokenType
#include <sstream>

std::vector<std::string> tokenize(const std::string& sentence) {
    Tokenizer tok;
    auto tokens = tok.tokenize(sentence);

    std::vector<std::string> words;
    for (const auto& t : tokens) {
        if (t.type == TokenType::WORD        ||
            t.type == TokenType::CONTRACTION ||
            t.type == TokenType::HYPHENATED) {
            words.push_back(t.text);
        }
    }

    if (words.empty()) {
        // Fallback: whitespace split
        std::istringstream ss(sentence);
        std::string w;
        while (ss >> w) words.push_back(w);
    }

    return words;
}
