#pragma once
#include "term.hpp"
#include "category.hpp"
#include <optional>
#include <vector>
#include <functional>

// ---------------------------------------------------------------------------
// When true, combination rules call beta_reduce after each step.
// Set to false to collect unreduced terms (for parse_steps).
// ---------------------------------------------------------------------------
bool  get_eager_reduce();
void  set_eager_reduce(bool v);

// RAII guard that restores eager_reduce on destruction
struct EagerGuard {
    bool saved;
    explicit EagerGuard(bool v) : saved(get_eager_reduce()) { set_eager_reduce(v); }
    ~EagerGuard() { set_eager_reduce(saved); }
};

// ---------------------------------------------------------------------------
// Combination result
// ---------------------------------------------------------------------------
struct CombineResult { Category cat; Term sem; };
using OptResult = std::optional<CombineResult>;

// ---------------------------------------------------------------------------
// Binary combination rules
// ---------------------------------------------------------------------------
OptResult forward_apply   (const Category&, const Term&, const Category&, const Term&);
OptResult backward_apply  (const Category&, const Term&, const Category&, const Term&);
OptResult forward_compose (const Category&, const Term&, const Category&, const Term&);
OptResult backward_compose(const Category&, const Term&, const Category&, const Term&);

using BinaryRule = std::function<OptResult(const Category&, const Term&,
                                            const Category&, const Term&)>;
extern const std::vector<BinaryRule> BINARY_RULES;

// ---------------------------------------------------------------------------
// Type raising
// ---------------------------------------------------------------------------
OptResult type_raise_forward (const Category&, const Term&);
OptResult type_raise_backward(const Category&, const Term&);
