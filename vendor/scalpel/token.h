#pragma once
#include <string>

enum class TokenType{
    WORD,
    NUMBER,
    PUNCT,
    ABBREVIATION,
    CONTRACTION,
    HYPHENATED,
    SENTENCE_END,
};

class Token {
    public:
    Token(std::string t, TokenType ty, int start, int end){
        text = t;
        type = ty;
        start_index = start;
        end_index = end;
    }

    Token(){
        text = " ";
        type = TokenType::WORD;
        start_index = 0;
        end_index = 0;
    }

    std::string text;
    TokenType type;
    int start_index;
    int end_index;
};
