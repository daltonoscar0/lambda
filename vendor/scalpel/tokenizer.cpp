#include "tokenizer.h"
#include "char_classes.h"
#include "token.h"
#include <string>
#include <vector>

Tokenizer::Tokenizer(){
    state_ = State::START;
    buffer_ = " ";
    token_start_ = 0;
}

void Tokenizer::reset(){
    state_ = State::START;
    buffer_ = " ";
    token_start_ = 0;
}

std::vector<Token> Tokenizer::tokenize(std::string input){
    std::vector<Token> tokens;

    for (int i=0; i<(int)input.length(); i++){
        process_char(input[i], i, tokens);
    }

    if (buffer_ != " "){
        emit_token(tokens, input.length() - 1, TokenType::WORD);
    }

    return tokens;
}

void Tokenizer::process_char(char c, int index, std::vector<Token>& output){
    CharClassifier classifier;
    CharClass cc = classifier.classify_char(c);

    if (state_ == State::START){
        if(cc == CharClass::LETTER){
            buffer_ = c;
            token_start_ = index;
            state_ = State::IN_WORD;
        } else if (cc == CharClass::DIGIT){
            buffer_ = c;
            token_start_ = index;
            state_ = State::IN_NUMBER;
        } else if (cc == CharClass::PUNCT || cc == CharClass::PERIOD){
            buffer_ = c;
            token_start_ = index;
            state_ = State::IN_PUNCT;
        }
    }

    else if (state_ == State::IN_WORD){
        if (cc == CharClass::LETTER){
            buffer_ = buffer_ + c;
        } else if (cc == CharClass::APOSTROPHE){
            buffer_ = buffer_ + c;
            state_ = State::IN_CONTRACTION;
        } else if (cc == CharClass::HYPHEN){
            buffer_ = buffer_ + c;
            state_ = State::IN_HYPHENATED;
        } else if (cc == CharClass::PERIOD && buffer_.length() <= 3){
            buffer_ = buffer_ + c;
            state_ = State::IN_ABBREVIATION;
        } else {
            emit_token(output, index - 1, TokenType::WORD);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }

    else if (state_ == State::IN_NUMBER){
        if (cc == CharClass::DIGIT){
            buffer_ = buffer_ + c;
        } else {
            emit_token(output, index - 1, TokenType::NUMBER);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }

    else if (state_ == State::IN_PUNCT){
        if (cc == CharClass::PUNCT || cc == CharClass::PERIOD){
            buffer_ = buffer_ + c;
        } else {
            emit_token(output, index - 1, TokenType::PUNCT);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }

    else if (state_ == State::IN_CONTRACTION){
        if (cc == CharClass::LETTER){
            buffer_ = buffer_ + c;
        } else {
            emit_token(output, index - 1, TokenType::CONTRACTION);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }

    else if (state_ == State::IN_ABBREVIATION){
        if (cc == CharClass::LETTER || cc == CharClass::PERIOD){
            buffer_ = buffer_ + c;
        } else {
            emit_token(output, index - 1, TokenType::ABBREVIATION);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }

    else if (state_ == State::IN_HYPHENATED){
        if (cc == CharClass::LETTER){
            buffer_ = buffer_ + c;
            state_ = State::IN_WORD;
        } else {
            emit_token(output, index - 1, TokenType::HYPHENATED);
            buffer_ = " ";
            state_ = State::START;
            process_char(c, index, output);
        }
    }
}

void Tokenizer::emit_token(std::vector<Token>& output, int end_index, TokenType type){
    Token t(buffer_, type, token_start_, end_index);
    output.push_back(t);
    buffer_ = " ";
    state_ = State::START;
}

void Tokenizer::transition_to(State next){
    state_ = next;
}

bool Tokenizer::is_abbreviation_candidate(std::string s){
    if (s.length() > 0 && s[s.length() - 1] == '.' && s.length() <= 4){
        return true;
    }
    return false;
}

bool Tokenizer::is_contraction_boundary(char prev, char curr){
    if (((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z')) &&
        ((curr >= 'a' && curr <= 'z') || (curr >= 'A' && curr <= 'Z'))) {
        return true;
    }
    return false;
}
