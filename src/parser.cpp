#include "parser.hpp"
#include "lambda_calc.hpp"
#include "ccg_rules.hpp"
#include "lexicon.hpp"
#include "tokenizer.hpp"
#include <map>
#include <set>
#include <utility>

// ---------------------------------------------------------------------------
// Chart type
// ---------------------------------------------------------------------------
using SpanKey = std::pair<int,int>;
struct SpanHash {
    size_t operator()(const SpanKey& k) const {
        return std::hash<int>()(k.first) * 31 + std::hash<int>()(k.second);
    }
};
using Chart = std::map<SpanKey, std::vector<ParseResult>>;

// Named binary rules (preserves order for deterministic parsing)
static const std::vector<std::pair<std::string, BinaryRule>> NAMED_RULES = {
    {"FA", forward_apply}, {"BA", backward_apply},
    {"FC", forward_compose}, {"BC", backward_compose}
};

// ---------------------------------------------------------------------------
// Deduplication helpers
// ---------------------------------------------------------------------------
static std::vector<ParseResult>
dedup(const std::vector<ParseResult>& entries) {
    std::set<std::pair<std::string,std::string>> seen;
    std::vector<ParseResult> result;
    for (auto& e : entries) {
        auto key = std::make_pair(cat_repr(e.cat), canonical_str(e.sem));
        if (seen.insert(key).second)
            result.push_back(e);
    }
    return result;
}

static std::vector<ParseResult>
dedup_by_structure(const std::vector<ParseResult>& entries) {
    std::set<std::string> seen;
    std::vector<ParseResult> result;
    for (auto& e : entries) {
        auto key = canonical_str(e.sem);
        if (seen.insert(key).second)
            result.push_back(e);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Apply type-raising to a list of entries (only NP -> lifted NP)
// ---------------------------------------------------------------------------
static std::vector<ParseResult>
apply_type_raising(const std::vector<ParseResult>& entries) {
    std::vector<ParseResult> out;
    for (auto& e : entries) {
        auto fwd = type_raise_forward(e.cat, e.sem);
        if (fwd) {
            auto node = std::make_shared<DerivNode>();
            node->rule = "TR_FWD";
            node->cat  = fwd->cat;
            node->children = {e.deriv};
            out.push_back({ fwd->cat, fwd->sem, node });
        }
        auto bwd = type_raise_backward(e.cat, e.sem);
        if (bwd) {
            auto node = std::make_shared<DerivNode>();
            node->rule = "TR_BWD";
            node->cat  = bwd->cat;
            node->children = {e.deriv};
            out.push_back({ bwd->cat, bwd->sem, node });
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Apply all binary rules between two entry lists
// ---------------------------------------------------------------------------
static std::vector<ParseResult>
apply_rules(const std::vector<ParseResult>& left,
            const std::vector<ParseResult>& right)
{
    std::vector<ParseResult> out;
    for (auto& l : left)
        for (auto& r : right)
            for (auto& [name, rule] : NAMED_RULES) {
                auto res = rule(l.cat, l.sem, r.cat, r.sem);
                if (res) {
                    auto node = std::make_shared<DerivNode>();
                    node->rule = name;
                    node->cat  = res->cat;
                    node->children = {l.deriv, r.deriv};
                    out.push_back({ res->cat, res->sem, node });
                }
            }
    return out;
}

// ---------------------------------------------------------------------------
// Core CYK chart parser
// ---------------------------------------------------------------------------
static std::vector<ParseResult>
parse_internal(const std::vector<std::string>& tokens) {
    int n = (int)tokens.size();
    Chart chart;

    // Single-token spans
    for (int i = 0; i < n; ++i) {
        auto lex = lexicon_lookup(tokens[i]);
        if (lex.empty())
            throw ParseError("No entry for '" + tokens[i] + "'", tokens);
        std::vector<ParseResult> entries;
        for (auto& e : lex) {
            auto node = std::make_shared<DerivNode>();
            node->rule = "LEX";
            node->word = tokens[i];
            node->cat  = e.cat;
            entries.push_back({ e.cat, e.sem, node });
        }
        auto raised = apply_type_raising(entries);
        entries.insert(entries.end(), raised.begin(), raised.end());
        chart[{i, i}] = dedup(entries);
    }

    // Multi-token spans
    for (int len = 2; len <= n; ++len) {
        for (int i = 0; i <= n - len; ++i) {
            int j = i + len - 1;
            std::vector<ParseResult> cell;
            for (int split = i; split < j; ++split) {
                auto& left  = chart[{i, split}];
                auto& right = chart[{split+1, j}];
                auto combined = apply_rules(left, right);
                cell.insert(cell.end(), combined.begin(), combined.end());
            }
            if (!cell.empty()) {
                // Apply type-raising to any NP results so multi-word NPs
                // (e.g. "a student") can participate in forward composition
                // with transitive verbs (needed for object relative clauses).
                auto raised = apply_type_raising(cell);
                cell.insert(cell.end(), raised.begin(), raised.end());
                chart[{i, j}] = dedup(cell);
            }
        }
    }

    auto it = chart.find({0, n-1});
    if (it == chart.end())
        throw ParseError("No parse found — no span covered the full sentence", tokens);

    std::vector<ParseResult> complete;
    for (auto& e : it->second)
        if (cat_equal(e.cat, CAT_S) || cat_equal(e.cat, CAT_Q))
            complete.push_back(e);

    if (complete.empty()) {
        std::string cats;
        for (auto& e : it->second) cats += " " + cat_repr(e.cat);
        throw ParseError("Sentence did not reduce to S or Q. Found:" + cats, tokens);
    }

    return dedup_by_structure(complete);
}

// ---------------------------------------------------------------------------
// Public parse()
// ---------------------------------------------------------------------------
std::vector<ParseResult> parse(const std::string& sentence) {
    reset_fresh_vars();
    auto tokens = tokenize(sentence);
    if (tokens.empty())
        throw ParseError("Empty input");
    return parse_internal(tokens);
}

// ---------------------------------------------------------------------------
// Public parse_steps()
// ---------------------------------------------------------------------------
std::vector<StepResult> parse_steps(const std::string& sentence) {
    reset_fresh_vars();
    auto tokens = tokenize(sentence);
    if (tokens.empty())
        throw ParseError("Empty input");

    EagerGuard guard(false);   // disable beta-reduce during parse
    auto results = parse_internal(tokens);

    std::set<std::string> seen;
    std::vector<StepResult> out;
    for (auto& r : results) {
        auto steps = beta_reduce_trace(r.sem);
        auto key   = canonical_str(steps.back());
        if (seen.insert(key).second)
            out.push_back({ r.sem, steps });
    }
    return out;
}
