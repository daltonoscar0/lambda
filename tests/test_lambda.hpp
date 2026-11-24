#pragma once
#include "test_runner.hpp"
#include "../src/lambda_calc.hpp"

inline void test_lambda(TestRunner& R) {

    // ------------------------------------------------------------------
    R.suite("free_vars");
    // ------------------------------------------------------------------
    CHECK(R, free_vars(make_var("x")) == std::set<std::string>{"x"});
    CHECK(R, free_vars(make_const("john")).empty());
    // λx. x, x is bound
    CHECK(R, free_vars(make_lam("x", make_var("x"))).empty());
    // λx. y, y is free
    CHECK(R, free_vars(make_lam("x", make_var("y"))) == std::set<std::string>{"y"});
    // f(x), both free
    { auto fv = free_vars(make_app(make_var("f"), make_var("x")));
      CHECK(R, fv.count("f") && fv.count("x")); }
    // ∀x. P(x), P is free, x is bound
    { auto fv = free_vars(make_forall("x", make_app(make_var("P"), make_var("x"))));
      CHECK(R, fv.count("P") && !fv.count("x")); }

    // ------------------------------------------------------------------
    R.suite("substitute");
    // ------------------------------------------------------------------
    // Var: x[x:=y] = y
    CHECK(R, term_equal(substitute(make_var("x"), "x", make_var("y")), make_var("y")));
    // Bound: (λx. x)[x:=y] = λx. x  (x is bound)
    { auto t = make_lam("x", make_var("x"));
      CHECK(R, term_equal(substitute(t, "x", make_var("y")), t)); }
    // No capture: (λy. x)[x:=y] must rename bound y
    { auto t = make_lam("y", make_var("x"));
      auto r = substitute(t, "x", make_var("y"));
      auto* lam = std::get_if<Lam>(&r->data);
      CHECK(R, lam && lam->var != "y");
      CHECK(R, lam && free_vars(lam->body).count("y")); }
    // Nested app: likes(x,z)[x:=john] = likes(john,z)
    { auto t = make_app(make_app(make_const("likes"), make_var("x")), make_var("z"));
      auto r = substitute(t, "x", make_const("john"));
      auto expected = make_app(make_app(make_const("likes"), make_const("john")), make_var("z"));
      CHECK(R, term_equal(r, expected)); }

    // ------------------------------------------------------------------
    R.suite("beta_reduce");
    // ------------------------------------------------------------------
    // (λx. x)(z) -> z
    { auto t = make_app(make_lam("x", make_var("x")), make_var("z"));
      CHECK(R, term_equal(beta_reduce(t), make_var("z"))); }
    // (λx. likes(john,x))(mary) -> likes(john,mary)
    { auto t = make_app(
          make_lam("x", make_app(make_app(make_const("likes"), make_const("john")), make_var("x"))),
          make_const("mary"));
      auto expected = make_app(make_app(make_const("likes"), make_const("john")), make_const("mary"));
      CHECK(R, term_equal(beta_reduce(t), expected)); }
    // Curried: (λx.λy.likes(x,y))(john)(mary) -> likes(john,mary)
    { auto t = make_app(
          make_app(
              make_lam("x", make_lam("y",
                  make_app(make_app(make_const("likes"), make_var("x")), make_var("y")))),
              make_const("john")),
          make_const("mary"));
      auto expected = make_app(make_app(make_const("likes"), make_const("john")), make_const("mary"));
      CHECK(R, term_equal(beta_reduce(t), expected)); }
    // Already normal form
    { auto t = make_app(make_const("f"), make_const("a"));
      CHECK(R, term_equal(beta_reduce(t), t)); }
    // Under lambda: λy. (λx.x)(y) -> λy. y
    { auto t = make_lam("y", make_app(make_lam("x", make_var("x")), make_var("y")));
      CHECK(R, term_equal(beta_reduce(t), make_lam("y", make_var("y")))); }

    // ------------------------------------------------------------------
    R.suite("beta_reduce_trace");
    // ------------------------------------------------------------------
    // Trace should have at least 2 entries for a reducible term
    { auto t = make_app(make_lam("x", make_var("x")), make_var("z"));
      auto steps = beta_reduce_trace(t);
      CHECK(R, steps.size() >= 2);
      CHECK(R, term_equal(steps.back(), make_var("z"))); }
    // Trace of normal form has exactly 1 entry
    { auto t = make_const("a");
      auto steps = beta_reduce_trace(t);
      CHECK_EQ(R, steps.size(), (size_t)1); }

    // ------------------------------------------------------------------
    R.suite("canonical_str");
    // ------------------------------------------------------------------
    // Two alpha-equivalent lambda terms have the same canonical string
    { auto t1 = make_lam("x", make_var("x"));
      auto t2 = make_lam("y", make_var("y"));
      CHECK(R, canonical_str(t1) == canonical_str(t2)); }
    // Different terms have different canonical strings
    { auto t1 = make_lam("x", make_var("x"));
      auto t2 = make_lam("x", make_const("a"));
      CHECK(R, canonical_str(t1) != canonical_str(t2)); }
}
