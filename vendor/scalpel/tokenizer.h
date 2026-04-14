#pragma once
#include "token.h"
#include <string>
#include <vector>

class Tokenizer {
public:
    Tokenizer();
    std::vector<Token> tokenize(std::string input);
    void reset();

private:
    enum class State {
        START,
        IN_WORD,
        IN_NUMBER,
        IN_ABBREVIATION,
        IN_CONTRACTION,
        IN_HYPHENATED,
        IN_PUNCT,
        POSSIBLE_SENTENCE_END
    };

    State state_;
    std::string buffer_;
    int token_start_;

    void process_char(char c, int index, std::vector<Token>& output);
    void emit_token(std::vector<Token>& output, int end_index, TokenType type);
    void transition_to(State next);
    bool is_abbreviation_candidate(std::string s);
    bool is_contraction_boundary(char prev, char curr);
};
