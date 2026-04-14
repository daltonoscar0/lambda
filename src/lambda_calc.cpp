#include "lambda_calc.hpp"
#include <stdexcept>
#include <map>

// ---------------------------------------------------------------------------
// Fresh variable generation
// ---------------------------------------------------------------------------
static const std::vector<std::string> VAR_NAMES = {
    "x","y","z","a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","p","q","r","s","t","u","v","w"
};
static int g_counter = 0;

void reset_fresh_vars() { g_counter = 0; }

std::string fresh_var(const std::set<std::string>& avoid) {
    while (true) {
        int n = g_counter++;
        std::string name = (n < (int)VAR_NAMES.size())
                           ? VAR_NAMES[n]
                           : (VAR_NAMES.back() + std::to_string(n - (int)VAR_NAMES.size() + 2));
        if (avoid.find(name) == avoid.end())
            return name;
    }
}

// ---------------------------------------------------------------------------
// Term equality
// ---------------------------------------------------------------------------
bool term_equal(const Term& a, const Term& b) {
    if (!a || !b) return a == b;
    return std::visit(overloaded{
        [&](const Var& va) -> bool {
            auto* vb = std::get_if<Var>(&b->data);
            return vb && va.name == vb->name;
        },
        [&](const Const& ca) -> bool {
            auto* cb = std::get_if<Const>(&b->data);
            return cb && ca.name == cb->name;
        },
        [&](const App& aa) -> bool {
            auto* ab = std::get_if<App>(&b->data);
            return ab && term_equal(aa.func, ab->func) && term_equal(aa.arg, ab->arg);
        },
        [&](const Lam& la) -> bool {
            auto* lb = std::get_if<Lam>(&b->data);
            if (!lb) return false;
            if (la.var == lb->var) return term_equal(la.body, lb->body);
            // Alpha-equivalence: rename lb's var to la's var in lb's body
            auto renamed = substitute(lb->body, lb->var, make_var(la.var));
            return term_equal(la.body, renamed);
        },
        [&](const Forall& fa) -> bool {
            auto* fb = std::get_if<Forall>(&b->data);
            return fb && fa.var == fb->var && term_equal(fa.body, fb->body);
        },
        [&](const Exists& ea) -> bool {
            auto* eb = std::get_if<Exists>(&b->data);
            return eb && ea.var == eb->var && term_equal(ea.body, eb->body);
        },
        [&](const And& aa) -> bool {
            auto* ab = std::get_if<And>(&b->data);
            return ab && term_equal(aa.left, ab->left) && term_equal(aa.right, ab->right);
        },
        [&](const Or& oa) -> bool {
            auto* ob = std::get_if<Or>(&b->data);
            return ob && term_equal(oa.left, ob->left) && term_equal(oa.right, ob->right);
        },
        [&](const Implies& ia) -> bool {
            auto* ib = std::get_if<Implies>(&b->data);
            return ib && term_equal(ia.left, ib->left) && term_equal(ia.right, ib->right);
        },
        [&](const Not& na) -> bool {
            auto* nb = std::get_if<Not>(&b->data);
            return nb && term_equal(na.body, nb->body);
        },
        [&](const Iota& ia) -> bool {
            auto* ib = std::get_if<Iota>(&b->data);
            if (!ib) return false;
            if (ia.var == ib->var) return term_equal(ia.body, ib->body);
            auto renamed = substitute(ib->body, ib->var, make_var(ia.var));
            return term_equal(ia.body, renamed);
        },
    }, a->data);
}

// ---------------------------------------------------------------------------
// Free variables
// ---------------------------------------------------------------------------
std::set<std::string> free_vars(const Term& t) {
    return std::visit(overloaded{
        [](const Var& v)    -> std::set<std::string> { return {v.name}; },
        [](const Const&)    -> std::set<std::string> { return {}; },
        [](const App& a)    -> std::set<std::string> {
            auto s = free_vars(a.func);
            auto r = free_vars(a.arg);
            s.insert(r.begin(), r.end());
            return s;
        },
        [](const Lam& l)    -> std::set<std::string> {
            auto s = free_vars(l.body);
            s.erase(l.var);
            return s;
        },
        [](const Forall& f) -> std::set<std::string> {
            auto s = free_vars(f.body);
            s.erase(f.var);
            return s;
        },
        [](const Exists& e) -> std::set<std::string> {
            auto s = free_vars(e.body);
            s.erase(e.var);
            return s;
        },
        [](const And& a)    -> std::set<std::string> {
            auto s = free_vars(a.left);
            auto r = free_vars(a.right);
            s.insert(r.begin(), r.end());
            return s;
        },
        [](const Or& o)     -> std::set<std::string> {
            auto s = free_vars(o.left);
            auto r = free_vars(o.right);
            s.insert(r.begin(), r.end());
            return s;
        },
        [](const Implies& i) -> std::set<std::string> {
            auto s = free_vars(i.left);
            auto r = free_vars(i.right);
            s.insert(r.begin(), r.end());
            return s;
        },
        [](const Not& n)    -> std::set<std::string> { return free_vars(n.body); },
        [](const Iota& i)   -> std::set<std::string> {
            auto s = free_vars(i.body);
            s.erase(i.var);
            return s;
        },
    }, t->data);
}

// ---------------------------------------------------------------------------
// Capture-avoiding substitution
// ---------------------------------------------------------------------------
Term substitute(const Term& t, const std::string& var, const Term& repl) {
    return std::visit(overloaded{
        [&](const Var& v) -> Term {
            return v.name == var ? repl : t;
        },
        [&](const Const&) -> Term { return t; },
        [&](const App& a) -> Term {
            return make_app(substitute(a.func, var, repl),
                            substitute(a.arg,  var, repl));
        },
        [&](const Lam& l) -> Term {
            if (l.var == var) return t;   // shadowed
            auto fv_repl = free_vars(repl);
            if (fv_repl.count(l.var)) {
                auto fv_body = free_vars(l.body);
                std::set<std::string> avoid = fv_repl;
                avoid.insert(fv_body.begin(), fv_body.end());
                avoid.insert(var);
                auto fresh = fresh_var(avoid);
                auto renamed = substitute(l.body, l.var, make_var(fresh));
                return make_lam(fresh, substitute(renamed, var, repl));
            }
            return make_lam(l.var, substitute(l.body, var, repl));
        },
        [&](const Forall& f) -> Term {
            if (f.var == var) return t;
            auto fv_repl = free_vars(repl);
            if (fv_repl.count(f.var)) {
                auto fv_body = free_vars(f.body);
                std::set<std::string> avoid = fv_repl;
                avoid.insert(fv_body.begin(), fv_body.end());
                avoid.insert(var);
                auto fresh = fresh_var(avoid);
                auto renamed = substitute(f.body, f.var, make_var(fresh));
                return make_forall(fresh, substitute(renamed, var, repl));
            }
            return make_forall(f.var, substitute(f.body, var, repl));
        },
        [&](const Exists& e) -> Term {
            if (e.var == var) return t;
            auto fv_repl = free_vars(repl);
            if (fv_repl.count(e.var)) {
                auto fv_body = free_vars(e.body);
                std::set<std::string> avoid = fv_repl;
                avoid.insert(fv_body.begin(), fv_body.end());
                avoid.insert(var);
                auto fresh = fresh_var(avoid);
                auto renamed = substitute(e.body, e.var, make_var(fresh));
                return make_exists(fresh, substitute(renamed, var, repl));
            }
            return make_exists(e.var, substitute(e.body, var, repl));
        },
        [&](const And& a) -> Term {
            return make_and(substitute(a.left, var, repl),
                            substitute(a.right, var, repl));
        },
        [&](const Or& o) -> Term {
            return make_or(substitute(o.left, var, repl),
                           substitute(o.right, var, repl));
        },
        [&](const Implies& i) -> Term {
            return make_implies(substitute(i.left, var, repl),
                                substitute(i.right, var, repl));
        },
        [&](const Not& n) -> Term {
            return make_not(substitute(n.body, var, repl));
        },
        [&](const Iota& i) -> Term {
            if (i.var == var) return t;
            auto fv_repl = free_vars(repl);
            if (fv_repl.count(i.var)) {
                auto fv_body = free_vars(i.body);
                std::set<std::string> avoid = fv_repl;
                avoid.insert(fv_body.begin(), fv_body.end());
                avoid.insert(var);
                auto fresh = fresh_var(avoid);
                auto renamed = substitute(i.body, i.var, make_var(fresh));
                return make_iota(fresh, substitute(renamed, var, repl));
            }
            return make_iota(i.var, substitute(i.body, var, repl));
        },
    }, t->data);
}

// ---------------------------------------------------------------------------
// Beta reduction
// ---------------------------------------------------------------------------
std::optional<Term> beta_reduce_once(const Term& t) {
    return std::visit(overloaded{
        [](const Var&)  -> std::optional<Term> { return std::nullopt; },
        [](const Const&)-> std::optional<Term> { return std::nullopt; },
        [&](const App& a) -> std::optional<Term> {
            // Beta redex: (λx. body)(arg)
            if (auto* lam = std::get_if<Lam>(&a.func->data))
                return substitute(lam->body, lam->var, a.arg);
            // Reduce func first
            if (auto r = beta_reduce_once(a.func))
                return make_app(*r, a.arg);
            // Then arg
            if (auto r = beta_reduce_once(a.arg))
                return make_app(a.func, *r);
            return std::nullopt;
        },
        [](const Lam& l) -> std::optional<Term> {
            if (auto r = beta_reduce_once(l.body))
                return make_lam(l.var, *r);
            return std::nullopt;
        },
        [](const Forall& f) -> std::optional<Term> {
            if (auto r = beta_reduce_once(f.body))
                return make_forall(f.var, *r);
            return std::nullopt;
        },
        [](const Exists& e) -> std::optional<Term> {
            if (auto r = beta_reduce_once(e.body))
                return make_exists(e.var, *r);
            return std::nullopt;
        },
        [](const And& a) -> std::optional<Term> {
            if (auto r = beta_reduce_once(a.left))  return make_and(*r, a.right);
            if (auto r = beta_reduce_once(a.right)) return make_and(a.left, *r);
            return std::nullopt;
        },
        [](const Or& o) -> std::optional<Term> {
            if (auto r = beta_reduce_once(o.left))  return make_or(*r, o.right);
            if (auto r = beta_reduce_once(o.right)) return make_or(o.left, *r);
            return std::nullopt;
        },
        [](const Implies& i) -> std::optional<Term> {
            if (auto r = beta_reduce_once(i.left))  return make_implies(*r, i.right);
            if (auto r = beta_reduce_once(i.right)) return make_implies(i.left, *r);
            return std::nullopt;
        },
        [](const Not& n) -> std::optional<Term> {
            if (auto r = beta_reduce_once(n.body)) return make_not(*r);
            return std::nullopt;
        },
        [](const Iota& i) -> std::optional<Term> {
            if (auto r = beta_reduce_once(i.body)) return make_iota(i.var, *r);
            return std::nullopt;
        },
    }, t->data);
}

Term beta_reduce(const Term& t, int max_steps) {
    Term current = t;
    for (int i = 0; i < max_steps; ++i) {
        auto r = beta_reduce_once(current);
        if (!r) return current;
        current = *r;
    }
    throw std::runtime_error("Beta reduction did not terminate");
}

std::vector<Term> beta_reduce_trace(const Term& t, int max_steps) {
    std::vector<Term> steps;
    Term current = t;
    for (int i = 0; i < max_steps; ++i) {
        steps.push_back(current);
        auto r = beta_reduce_once(current);
        if (!r) return steps;
        current = *r;
    }
    throw std::runtime_error("Beta reduction did not terminate");
}

// ---------------------------------------------------------------------------
// Canonical string (bound vars renamed to positional names _0, _1, ...)
// ---------------------------------------------------------------------------
static std::string canonical_str_impl(const Term& t,
                                      std::map<std::string,std::string>& map,
                                      int& counter)
{
    return std::visit(overloaded{
        [&](const Var& v) -> std::string {
            auto it = map.find(v.name);
            return it != map.end() ? it->second : v.name;
        },
        [&](const Const& c) -> std::string { return "#" + c.name; },
        [&](const App& a) -> std::string {
            return "@(" + canonical_str_impl(a.func, map, counter)
                 + "," + canonical_str_impl(a.arg,  map, counter) + ")";
        },
        [&](const Lam& l) -> std::string {
            auto fresh = "_" + std::to_string(counter++);
            auto old_map = map;
            map[l.var] = fresh;
            auto body = canonical_str_impl(l.body, map, counter);
            map = old_map;
            return "L" + fresh + "." + body;
        },
        [&](const Forall& f) -> std::string {
            auto fresh = "_" + std::to_string(counter++);
            auto old_map = map;
            map[f.var] = fresh;
            auto body = canonical_str_impl(f.body, map, counter);
            map = old_map;
            return "A" + fresh + "." + body;
        },
        [&](const Exists& e) -> std::string {
            auto fresh = "_" + std::to_string(counter++);
            auto old_map = map;
            map[e.var] = fresh;
            auto body = canonical_str_impl(e.body, map, counter);
            map = old_map;
            return "E" + fresh + "." + body;
        },
        [&](const And& a) -> std::string {
            return "&(" + canonical_str_impl(a.left, map, counter)
                 + "," + canonical_str_impl(a.right, map, counter) + ")";
        },
        [&](const Or& o) -> std::string {
            return "|(" + canonical_str_impl(o.left, map, counter)
                 + "," + canonical_str_impl(o.right, map, counter) + ")";
        },
        [&](const Implies& i) -> std::string {
            return ">(" + canonical_str_impl(i.left, map, counter)
                 + "," + canonical_str_impl(i.right, map, counter) + ")";
        },
        [&](const Not& n) -> std::string {
            return "!" + canonical_str_impl(n.body, map, counter);
        },
        [&](const Iota& i) -> std::string {
            auto fresh = "_" + std::to_string(counter++);
            auto old_map = map;
            map[i.var] = fresh;
            auto body = canonical_str_impl(i.body, map, counter);
            map = old_map;
            return "I" + fresh + "." + body;
        },
    }, t->data);
}

std::string canonical_str(const Term& t) {
    std::map<std::string,std::string> m;
    int counter = 0;
    return canonical_str_impl(t, m, counter);
}
