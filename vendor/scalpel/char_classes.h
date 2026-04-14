#pragma once

enum class CharClass{
    LETTER,
    DIGIT,
    WHITESPACE,
    PERIOD,
    APOSTROPHE,
    HYPHEN,
    PUNCT,
    OTHER
};

class CharClassifier{
    public:
    CharClass classify_char(char c);
};
