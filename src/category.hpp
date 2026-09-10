#pragma once
#include "utils.hpp"
#include <memory>
#include <string>
#include <variant>

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------
struct CatNode;
using Category = std::shared_ptr<CatNode>;

// ---------------------------------------------------------------------------
// Node structs
// ---------------------------------------------------------------------------
struct AtomCat    { std::string name; };
struct ForwardCat { Category result; Category arg; };   // A/B
struct BackwardCat{ Category result; Category arg; };   // A\B

using CatVariant = std::variant<AtomCat, ForwardCat, BackwardCat>;

struct CatNode {
    CatVariant data;
    explicit CatNode(CatVariant d) : data(std::move(d)) {}
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------
inline Category make_atom(std::string n)             { return std::make_shared<CatNode>(AtomCat{std::move(n)}); }
inline Category make_forward(Category r, Category a) { return std::make_shared<CatNode>(ForwardCat{std::move(r), std::move(a)}); }
inline Category make_backward(Category r, Category a){ return std::make_shared<CatNode>(BackwardCat{std::move(r), std::move(a)}); }

// ---------------------------------------------------------------------------
// Pre-built standard categories (defined in category.cpp)
// ---------------------------------------------------------------------------
extern const Category CAT_S;
extern const Category CAT_N;
extern const Category CAT_NP;
extern const Category CAT_PP;
extern const Category CAT_CONJ;
extern const Category CAT_Q;    // question sentence

// Derived common categories, built on first use
Category cat_SbNP_fNP();  // (S\NP)/NP  transitive verb
Category cat_SbNP();      // S\NP       intransitive verb / VP
Category cat_NPfN();      // NP/N       determiner

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------
bool       cat_equal(const Category& a, const Category& b);
std::string cat_repr(const Category& c);
Category   parse_category(const std::string& s);
