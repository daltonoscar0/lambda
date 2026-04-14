/**
 * tokenize_words — CCG-lambda bridge to Scalpel
 *
 * Reads a sentence from stdin, tokenizes it with Scalpel,
 * and prints each WORD / CONTRACTION / HYPHENATED token text
 * on its own line (one token per line, no type label).
 *
 * PUNCT, NUMBER, ABBREVIATION, SENTENCE_END tokens are silently
 * dropped — the CCG parser only cares about word-class tokens.
 */
#include <iostream>
#include <string>
#include <vector>
#include "tokenizer.h"
#include "token.h"

int main(){
    std::string input;
    std::getline(std::cin, input);

    Tokenizer tokenizer;
    std::vector<Token> tokens = tokenizer.tokenize(input);

    for (const Token& t : tokens){
        if (t.type == TokenType::WORD        ||
            t.type == TokenType::CONTRACTION ||
            t.type == TokenType::HYPHENATED) {
            std::cout << t.text << "\n";
        }
    }
    return 0;
}
