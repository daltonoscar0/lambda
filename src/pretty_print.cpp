#include "pretty_print.hpp"
#include <map>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Precedence levels
// ---------------------------------------------------------------------------
static constexpr int PREC_IMPLIES = 1;
static constexpr int PREC_OR      = 2;
static constexpr int PREC_AND     = 3;
static constexpr int PREC_NOT     = 4;
static constexpr int PREC_QUANT   = 5;  // ∀ ∃ λ ι
static constexpr int PREC_APP     = 6;
static constexpr int PREC_ATOM    = 7;

// ---------------------------------------------------------------------------
// Canonical variable renaming
// ---------------------------------------------------------------------------
static const std::vector<std::string> CANON_VARS = {
    "x","y","z","a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","p","q","r","s","t","u","v","w"
};

static Term rename_bound(const Term& t,
                         std::map<std::string,std::string>& mp,
                         int& counter)
{
    auto new_name = [&]() -> std::string {
        int n = counter++;
        return n < (int)CANON_VARS.size() ? CANON_VARS[n]
                                          : ("v" + std::to_string(n));
    };

    return std::visit(overloaded{
        [&](const Var& v) -> Term {
            auto it = mp.find(v.name);
            return it != mp.end() ? make_var(it->second) : t;
        },
        [&](const Const&) -> Term { return t; },
        [&](const App& a) -> Term {
            return make_app(rename_bound(a.func, mp, counter),
                            rename_bound(a.arg,  mp, counter));
        },
        [&](const Lam& l) -> Term {
            auto n = new_name();
            auto old = mp[l.var]; mp[l.var] = n;
            auto body = rename_bound(l.body, mp, counter);
            mp[l.var] = old;
            return make_lam(n, body);
        },
        [&](const Forall& f) -> Term {
            auto n = new_name();
            auto old = mp[f.var]; mp[f.var] = n;
            auto body = rename_bound(f.body, mp, counter);
            mp[f.var] = old;
            return make_forall(n, body);
        },
        [&](const Exists& e) -> Term {
            auto n = new_name();
            auto old = mp[e.var]; mp[e.var] = n;
            auto body = rename_bound(e.body, mp, counter);
            mp[e.var] = old;
            return make_exists(n, body);
        },
        [&](const Iota& i) -> Term {
            auto n = new_name();
            auto old = mp[i.var]; mp[i.var] = n;
            auto body = rename_bound(i.body, mp, counter);
            mp[i.var] = old;
            return make_iota(n, body);
        },
        [&](const And& a) -> Term {
            return make_and(rename_bound(a.left,  mp, counter),
                            rename_bound(a.right, mp, counter));
        },
        [&](const Or& o) -> Term {
            return make_or(rename_bound(o.left,  mp, counter),
                           rename_bound(o.right, mp, counter));
        },
        [&](const Implies& i) -> Term {
            return make_implies(rename_bound(i.left,  mp, counter),
                                rename_bound(i.right, mp, counter));
        },
        [&](const Not& n) -> Term {
            return make_not(rename_bound(n.body, mp, counter));
        },
    }, t->data);
}

// ---------------------------------------------------------------------------
// Internal printer
// ---------------------------------------------------------------------------
static std::string pp(const Term& t, int prec);

static std::string wrap(const std::string& s, int own, int ctx) {
    return own < ctx ? "(" + s + ")" : s;
}

static bool is_atom_term(const Term& t) {
    return std::holds_alternative<Var>(t->data) ||
           std::holds_alternative<Const>(t->data);
}

// Uncurry: App(App(f, a), b) → (f, [a, b])
static std::pair<Term, std::vector<Term>> uncurry(const Term& t) {
    std::vector<Term> args;
    Term cur = t;
    while (auto* a = std::get_if<App>(&cur->data)) {
        args.push_back(a->arg);
        cur = a->func;
    }
    std::reverse(args.begin(), args.end());
    return { cur, args };
}

// Strip nested Lam: λx. λy. body → ("x y", body)
static std::pair<std::string, Term> strip_lam(const Term& t) {
    std::string vars;
    Term cur = t;
    while (auto* l = std::get_if<Lam>(&cur->data)) {
        if (!vars.empty()) vars += " ";
        vars += l->var;
        cur = l->body;
    }
    return { vars, cur };
}

static std::pair<std::string, Term> strip_quant(const Term& t, bool forall) {
    std::string vars;
    Term cur = t;
    while (true) {
        if (forall) {
            auto* f = std::get_if<Forall>(&cur->data);
            if (!f) break;
            if (!vars.empty()) vars += " ";
            vars += f->var;
            cur = f->body;
        } else {
            auto* e = std::get_if<Exists>(&cur->data);
            if (!e) break;
            if (!vars.empty()) vars += " ";
            vars += e->var;
            cur = e->body;
        }
    }
    return { vars, cur };
}

static std::string pp(const Term& t, int prec) {
    return std::visit(overloaded{
        [&](const Var& v) -> std::string { return v.name; },
        [&](const Const& c) -> std::string { return c.name; },
        [&](const App&) -> std::string {
            auto [func, args] = uncurry(t);
            std::string s;
            if (std::holds_alternative<Const>(func->data)) {
                // Predicate style: pred(a, b)
                std::string args_str;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i) args_str += ", ";
                    args_str += pp(args[i], 0);
                }
                s = pp(func, PREC_APP) + "(" + args_str + ")";
            } else {
                // Space-application: (f) a b
                s = pp(func, PREC_APP);
                for (auto& a : args) {
                    s += " ";
                    s += is_atom_term(a) ? pp(a, 0) : "(" + pp(a, 0) + ")";
                }
            }
            return wrap(s, PREC_APP, prec);
        },
        [&](const Lam&) -> std::string {
            auto [vars, body] = strip_lam(t);
            auto s = "\u03bb" + vars + ". " + pp(body, 0);
            return wrap(s, PREC_QUANT, prec);
        },
        [&](const Forall&) -> std::string {
            auto [vars, body] = strip_quant(t, true);
            auto s = "\u2200" + vars + ". (" + pp(body, 0) + ")";
            return wrap(s, PREC_QUANT, prec);
        },
        [&](const Exists&) -> std::string {
            auto [vars, body] = strip_quant(t, false);
            auto s = "\u2203" + vars + ". (" + pp(body, 0) + ")";
            return wrap(s, PREC_QUANT, prec);
        },
        [&](const And& a) -> std::string {
            auto s = pp(a.left, PREC_AND) + " \u2227 " + pp(a.right, PREC_AND - 1);
            return wrap(s, PREC_AND, prec);
        },
        [&](const Or& o) -> std::string {
            auto s = pp(o.left, PREC_OR) + " \u2228 " + pp(o.right, PREC_OR - 1);
            return wrap(s, PREC_OR, prec);
        },
        [&](const Implies& i) -> std::string {
            auto s = pp(i.left, PREC_IMPLIES + 1) + " \u2192 " + pp(i.right, PREC_IMPLIES);
            return wrap(s, PREC_IMPLIES, prec);
        },
        [&](const Not& n) -> std::string {
            auto s = "\u00ac" + pp(n.body, PREC_NOT);
            return wrap(s, PREC_NOT, prec);
        },
        [&](const Iota& i) -> std::string {
            auto s = "\u03b9" + i.var + ". " + pp(i.body, 0);
            return wrap(s, PREC_QUANT, prec);
        },
    }, t->data);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
std::string pretty(const Term& t) {
    std::map<std::string,std::string> mp;
    int counter = 0;
    auto canonical = rename_bound(t, mp, counter);
    return pp(canonical, 0);
}
