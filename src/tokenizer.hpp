#pragma once
#include <string>
#include <vector>

// Tokenize sentence using Scalpel directly (no subprocess).
// Returns word-class tokens (WORD, CONTRACTION, HYPHENATED).
// PUNCT, NUMBER, ABBREVIATION, SENTENCE_END are dropped.
std::vector<std::string> tokenize(const std::string& sentence);
