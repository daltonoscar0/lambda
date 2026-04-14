#pragma once
#include "term.hpp"
#include "category.hpp"
#include <string>
#include <vector>

struct LexEntry { Category cat; Term sem; };

std::vector<LexEntry> lexicon_lookup(const std::string& word);
