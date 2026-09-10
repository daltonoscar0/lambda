#pragma once
#include "term.hpp"
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Fresh variable generation
// ---------------------------------------------------------------------------
void        reset_fresh_vars();
std::string fresh_var(const std::set<std::string>& avoid = {});

// ---------------------------------------------------------------------------
// Term equality (structural; Lam uses alpha-equivalence)
// ---------------------------------------------------------------------------
bool term_equal(const Term& a, const Term& b);

// ---------------------------------------------------------------------------
// Free variables
// ---------------------------------------------------------------------------
std::set<std::string> free_vars(const Term& t);

// ---------------------------------------------------------------------------
// Capture-avoiding substitution: replace free var `v` with `replacement`
// ---------------------------------------------------------------------------
Term substitute(const Term& t, const std::string& v, const Term& replacement);

// ---------------------------------------------------------------------------
// Beta reduction
// ---------------------------------------------------------------------------
std::optional<Term> beta_reduce_once(const Term& t);
Term                beta_reduce(const Term& t, int max_steps = 500);
std::vector<Term>   beta_reduce_trace(const Term& t, int max_steps = 500);

// ---------------------------------------------------------------------------
// Canonical string, used for structural deduplication (bound vars renamed
// depth-first to positional names _0, _1, ...)
// ---------------------------------------------------------------------------
std::string canonical_str(const Term& t);
