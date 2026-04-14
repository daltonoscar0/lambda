#include "category.hpp"
#include <stdexcept>
#include <cassert>

// ---------------------------------------------------------------------------
// Standard categories
// ---------------------------------------------------------------------------
const Category CAT_S    = make_atom("S");
const Category CAT_N    = make_atom("N");
const Category CAT_NP   = make_atom("NP");
const Category CAT_PP   = make_atom("PP");
const Category CAT_CONJ = make_atom("CONJ");
const Category CAT_Q    = make_atom("Q");

Category cat_SbNP_fNP() {
    // (S\NP)/NP
    return make_forward(make_backward(CAT_S, CAT_NP), CAT_NP);
}
Category cat_SbNP() {
    return make_backward(CAT_S, CAT_NP);
}
Category cat_NPfN() {
    return make_forward(CAT_NP, CAT_N);
}

// ---------------------------------------------------------------------------
// Equality
// ---------------------------------------------------------------------------
bool cat_equal(const Category& a, const Category& b) {
    if (!a || !b) return a == b;
    return std::visit([&](const auto& va) -> bool {
        using T = std::decay_t<decltype(va)>;
        auto* vb = std::get_if<T>(&b->data);
        if (!vb) return false;
        if constexpr (std::is_same_v<T, AtomCat>)
            return va.name == vb->name;
        else
            return cat_equal(va.result, vb->result) && cat_equal(va.arg, vb->arg);
    }, a->data);
}

// ---------------------------------------------------------------------------
// String representation
// ---------------------------------------------------------------------------
std::string cat_repr(const Category& c) {
    return std::visit(overloaded{
        [](const AtomCat& a) -> std::string { return a.name; },
        [](const ForwardCat& f) -> std::string {
            auto r = cat_repr(f.result);
            auto a = cat_repr(f.arg);
            if (std::holds_alternative<ForwardCat>(f.result->data) ||
                std::holds_alternative<BackwardCat>(f.result->data))
                r = "(" + r + ")";
            if (std::holds_alternative<ForwardCat>(f.arg->data) ||
                std::holds_alternative<BackwardCat>(f.arg->data))
                a = "(" + a + ")";
            return r + "/" + a;
        },
        [](const BackwardCat& b) -> std::string {
            auto r = cat_repr(b.result);
            auto a = cat_repr(b.arg);
            if (std::holds_alternative<ForwardCat>(b.result->data) ||
                std::holds_alternative<BackwardCat>(b.result->data))
                r = "(" + r + ")";
            if (std::holds_alternative<ForwardCat>(b.arg->data) ||
                std::holds_alternative<BackwardCat>(b.arg->data))
                a = "(" + a + ")";
            return r + "\\" + a;
        },
    }, c->data);
}

// ---------------------------------------------------------------------------
// Parse category string (recursive descent)
// ---------------------------------------------------------------------------
static Category parse_cat_str(const std::string& s, size_t lo, size_t hi);

static size_t matching_paren(const std::string& s, size_t open) {
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') { --depth; if (depth == 0) return i; }
    }
    return std::string::npos;
}

static Category parse_cat_str(const std::string& s, size_t lo, size_t hi) {
    // Strip surrounding whitespace
    while (lo < hi && s[lo] == ' ') ++lo;
    while (hi > lo && s[hi-1] == ' ') --hi;

    // Strip outer parens
    if (s[lo] == '(' && matching_paren(s, lo) == hi - 1)
        return parse_cat_str(s, lo + 1, hi - 1);

    // Find top-level slash (scan right-to-left, outside parens)
    int depth = 0;
    for (size_t i = hi; i-- > lo; ) {
        if (s[i] == ')') ++depth;
        else if (s[i] == '(') --depth;
        else if ((s[i] == '/' || s[i] == '\\') && depth == 0) {
            auto left  = parse_cat_str(s, lo, i);
            auto right = parse_cat_str(s, i + 1, hi);
            if (s[i] == '/') return make_forward(left, right);
            else             return make_backward(left, right);
        }
    }

    // Atomic
    std::string name = s.substr(lo, hi - lo);
    if (name == "S")    return CAT_S;
    if (name == "N")    return CAT_N;
    if (name == "NP")   return CAT_NP;
    if (name == "PP")   return CAT_PP;
    if (name == "CONJ") return CAT_CONJ;
    if (name == "Q")    return CAT_Q;
    return make_atom(name);
}

Category parse_category(const std::string& s) {
    return parse_cat_str(s, 0, s.size());
}

