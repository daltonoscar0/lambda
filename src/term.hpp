#pragma once
#include <memory>
#include <string>
#include <variant>

// ---------------------------------------------------------------------------
// Forward declaration — allows Term to be used inside node structs
// ---------------------------------------------------------------------------
struct TermNode;
using Term = std::shared_ptr<TermNode>;

// ---------------------------------------------------------------------------
// Node structs (all recursive fields are Term = shared_ptr<TermNode>)
// ---------------------------------------------------------------------------
struct Var     { std::string name; };
struct Const   { std::string name; };
struct App     { Term func; Term arg; };
struct Lam     { std::string var; Term body; };
struct Forall  { std::string var; Term body; };
struct Exists  { std::string var; Term body; };
struct And     { Term left; Term right; };
struct Or      { Term left; Term right; };
struct Implies { Term left; Term right; };
struct Not     { Term body; };
struct Iota    { std::string var; Term body; };  // ιx. P(x) — definite description

using TermVariant = std::variant<Var, Const, App, Lam, Forall, Exists,
                                  And, Or, Implies, Not, Iota>;

struct TermNode {
    TermVariant data;
    explicit TermNode(TermVariant d) : data(std::move(d)) {}
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------
inline Term make_var(std::string n)                   { return std::make_shared<TermNode>(Var{std::move(n)}); }
inline Term make_const(std::string n)                 { return std::make_shared<TermNode>(Const{std::move(n)}); }
inline Term make_app(Term f, Term x)                  { return std::make_shared<TermNode>(App{std::move(f), std::move(x)}); }
inline Term make_lam(std::string v, Term b)           { return std::make_shared<TermNode>(Lam{std::move(v), std::move(b)}); }
inline Term make_forall(std::string v, Term b)        { return std::make_shared<TermNode>(Forall{std::move(v), std::move(b)}); }
inline Term make_exists(std::string v, Term b)        { return std::make_shared<TermNode>(Exists{std::move(v), std::move(b)}); }
inline Term make_and(Term l, Term r)                  { return std::make_shared<TermNode>(And{std::move(l), std::move(r)}); }
inline Term make_or(Term l, Term r)                   { return std::make_shared<TermNode>(Or{std::move(l), std::move(r)}); }
inline Term make_implies(Term l, Term r)              { return std::make_shared<TermNode>(Implies{std::move(l), std::move(r)}); }
inline Term make_not(Term b)                          { return std::make_shared<TermNode>(Not{std::move(b)}); }
inline Term make_iota(std::string v, Term b)          { return std::make_shared<TermNode>(Iota{std::move(v), std::move(b)}); }

#include "utils.hpp"
