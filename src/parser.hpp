#pragma once
#include "term.hpp"
#include "category.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Derivation tree
// ---------------------------------------------------------------------------
struct DerivNode {
    std::string rule;   // "LEX", "TR_FWD", "TR_BWD", "FA", "BA", "FC", "BC"
    std::string word;   // non-empty only for LEX nodes
    Category    cat;
    std::vector<std::shared_ptr<DerivNode>> children;
};
using DerivTree = std::shared_ptr<DerivNode>;

// ---------------------------------------------------------------------------
// Parse result
// ---------------------------------------------------------------------------
struct ParseResult {
    Category cat;
    Term     sem;
    DerivTree deriv;   // derivation tree (may be null for internal use)
};

// ---------------------------------------------------------------------------
// Parse error
// ---------------------------------------------------------------------------
struct ParseError : std::runtime_error {
    std::vector<std::string> tokens;
    explicit ParseError(const std::string& msg,
                        std::vector<std::string> toks = {})
        : std::runtime_error(msg), tokens(std::move(toks)) {}
};

// ---------------------------------------------------------------------------
// Parse — returns all S/Q-spanning results, deduplicated by structure
// ---------------------------------------------------------------------------
std::vector<ParseResult> parse(const std::string& sentence);

// ---------------------------------------------------------------------------
// Parse with step tracing — returns raw term + reduction steps per parse
// ---------------------------------------------------------------------------
struct StepResult { Term raw; std::vector<Term> steps; };
std::vector<StepResult> parse_steps(const std::string& sentence);
