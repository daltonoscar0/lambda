#pragma once
#include <string>
#include <vector>
#include "token.h"

class SentenceSegmenter{
    public:
    std::vector<std::vector<Token>> segment(std::vector<Token> tokens);

    private:
    bool is_sentence_end(Token t);
    std::vector<std::string> abbreviations;
};
