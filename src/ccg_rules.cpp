#include "ccg_rules.hpp"
#include "lambda_calc.hpp"

// ---------------------------------------------------------------------------
// Eager reduce flag
// ---------------------------------------------------------------------------
static bool g_eager_reduce = true;
bool  get_eager_reduce()      { return g_eager_reduce; }
void  set_eager_reduce(bool v){ g_eager_reduce = v; }

static Term maybe_reduce(Term t) {
    return g_eager_reduce ? beta_reduce(t) : t;
}

// ---------------------------------------------------------------------------
// Forward Application: (A/B) B → A     sem: f(x)
// ---------------------------------------------------------------------------
OptResult forward_apply(const Category& lc, const Term& ls,
                        const Category& rc, const Term& rs)
{
    auto* fwd = std::get_if<ForwardCat>(&lc->data);
    if (!fwd || !cat_equal(fwd->arg, rc)) return std::nullopt;
    return CombineResult{ fwd->result, maybe_reduce(make_app(ls, rs)) };
}

// ---------------------------------------------------------------------------
// Backward Application: B (A\B) → A    sem: f(x)
// ---------------------------------------------------------------------------
OptResult backward_apply(const Category& lc, const Term& ls,
                         const Category& rc, const Term& rs)
{
    auto* bwd = std::get_if<BackwardCat>(&rc->data);
    if (!bwd || !cat_equal(bwd->arg, lc)) return std::nullopt;
    return CombineResult{ bwd->result, maybe_reduce(make_app(rs, ls)) };
}

// ---------------------------------------------------------------------------
// Forward Composition: (A/B)(B/C) → A/C    sem: λx. f(g(x))
// ---------------------------------------------------------------------------
OptResult forward_compose(const Category& lc, const Term& ls,
                          const Category& rc, const Term& rs)
{
    auto* lf = std::get_if<ForwardCat>(&lc->data);
    auto* rf = std::get_if<ForwardCat>(&rc->data);
    if (!lf || !rf || !cat_equal(lf->arg, rf->result)) return std::nullopt;
    auto x = fresh_var();
    auto sem = maybe_reduce(make_lam(x, make_app(ls, make_app(rs, make_var(x)))));
    return CombineResult{ make_forward(lf->result, rf->arg), sem };
}

// ---------------------------------------------------------------------------
// Backward Composition: (B\C)(A\B) → A\C    sem: λx. g(f(x))
// ---------------------------------------------------------------------------
OptResult backward_compose(const Category& lc, const Term& ls,
                           const Category& rc, const Term& rs)
{
    auto* lb = std::get_if<BackwardCat>(&lc->data);
    auto* rb = std::get_if<BackwardCat>(&rc->data);
    if (!lb || !rb || !cat_equal(rb->arg, lb->result)) return std::nullopt;
    auto x = fresh_var();
    auto sem = maybe_reduce(make_lam(x, make_app(rs, make_app(ls, make_var(x)))));
    return CombineResult{ make_backward(rb->result, lb->arg), sem };
}

// ---------------------------------------------------------------------------
// Binary rules list
// ---------------------------------------------------------------------------
const std::vector<BinaryRule> BINARY_RULES = {
    forward_apply, backward_apply, forward_compose, backward_compose
};

// ---------------------------------------------------------------------------
// Type raising
// ---------------------------------------------------------------------------
OptResult type_raise_forward(const Category& cat, const Term& sem) {
    if (!cat_equal(cat, CAT_NP)) return std::nullopt;
    auto f = fresh_var();
    auto raised = maybe_reduce(make_lam(f, make_app(make_var(f), sem)));
    return CombineResult{ make_forward(CAT_S, make_backward(CAT_S, CAT_NP)), raised };
}

OptResult type_raise_backward(const Category& cat, const Term& sem) {
    if (!cat_equal(cat, CAT_NP)) return std::nullopt;
    auto f = fresh_var();
    auto raised = maybe_reduce(make_lam(f, make_app(make_var(f), sem)));
    return CombineResult{ make_backward(CAT_S, make_forward(CAT_S, CAT_NP)), raised };
}
