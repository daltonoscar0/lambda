#pragma once
#include "test_runner.hpp"
#include "../src/parser.hpp"
#include "../src/pretty_print.hpp"
#include <algorithm>
#include <string>

static bool contains(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

inline void test_parser(TestRunner& R) {

    // ------------------------------------------------------------------
    R.suite("parser — every student likes a cat");
    // ------------------------------------------------------------------
    {
        auto results = parse("Every student likes a cat");
        CHECK(R, !results.empty());
        auto pp = pretty(results[0].sem);
        CHECK(R, contains(pp, "\u2200"));   // ∀
        CHECK(R, contains(pp, "student"));
        CHECK(R, contains(pp, "\u2192"));   // →
        CHECK(R, contains(pp, "\u2203"));   // ∃
        CHECK(R, contains(pp, "cat"));
        CHECK(R, contains(pp, "\u2227"));   // ∧
        CHECK(R, contains(pp, "like"));
        // Structural: top level is Forall
        CHECK(R, std::holds_alternative<Forall>(results[0].sem->data));
        auto& fa = std::get<Forall>(results[0].sem->data);
        CHECK(R, std::holds_alternative<Implies>(fa.body->data));
        auto& imp = std::get<Implies>(fa.body->data);
        // Antecedent is student(x)
        auto* app = std::get_if<App>(&imp.left->data);
        CHECK(R, app && std::holds_alternative<Const>(app->func->data));
        if (app) {
            CHECK(R, std::get<Const>(app->func->data).name == "student");
        }
        // Consequent is Exists
        CHECK(R, std::holds_alternative<Exists>(imp.right->data));
    }

    // ------------------------------------------------------------------
    R.suite("parser — some person sees every dog");
    // ------------------------------------------------------------------
    {
        auto results = parse("Some person sees every dog");
        CHECK(R, !results.empty());
        auto pp = pretty(results[0].sem);
        CHECK(R, contains(pp, "\u2203"));
        CHECK(R, contains(pp, "person"));
        CHECK(R, contains(pp, "\u2200"));
        CHECK(R, contains(pp, "dog"));
        CHECK(R, contains(pp, "see"));
    }

    // ------------------------------------------------------------------
    R.suite("parser — no cat likes a dog");
    // ------------------------------------------------------------------
    {
        auto results = parse("No cat likes a dog");
        CHECK(R, !results.empty());
        auto pp = pretty(results[0].sem);
        CHECK(R, contains(pp, "\u00ac"));   // ¬
        CHECK(R, contains(pp, "\u2203"));
        CHECK(R, contains(pp, "cat"));
        CHECK(R, contains(pp, "dog"));
        CHECK(R, contains(pp, "like"));
        // Structural: top level is Not
        CHECK(R, std::holds_alternative<Not>(results[0].sem->data));
        auto& nt = std::get<Not>(results[0].sem->data);
        CHECK(R, std::holds_alternative<Exists>(nt.body->data));
    }

    // ------------------------------------------------------------------
    R.suite("parser — a student likes every cat");
    // ------------------------------------------------------------------
    {
        auto results = parse("A student likes every cat");
        CHECK(R, !results.empty());
        auto pp = pretty(results[0].sem);
        CHECK(R, contains(pp, "\u2203"));
        CHECK(R, contains(pp, "student"));
        CHECK(R, contains(pp, "\u2200"));
        CHECK(R, contains(pp, "cat"));
        CHECK(R, contains(pp, "like"));
    }

    // ------------------------------------------------------------------
    R.suite("parser — intransitive");
    // ------------------------------------------------------------------
    {
        auto r1 = parse("Every dog runs");
        CHECK(R, !r1.empty());
        auto pp1 = pretty(r1[0].sem);
        CHECK(R, contains(pp1, "\u2200") && contains(pp1, "dog") && contains(pp1, "run"));

        auto r2 = parse("Some cat sleeps");
        CHECK(R, !r2.empty());
        auto pp2 = pretty(r2[0].sem);
        CHECK(R, contains(pp2, "\u2203") && contains(pp2, "cat") && contains(pp2, "sleep"));
    }

    // ------------------------------------------------------------------
    R.suite("parser — proper name");
    // ------------------------------------------------------------------
    {
        auto results = parse("john likes a cat");
        CHECK(R, !results.empty());
        auto pp = pretty(results[0].sem);
        CHECK(R, contains(pp, "john") && contains(pp, "cat") && contains(pp, "like"));
    }

    // ------------------------------------------------------------------
    R.suite("parser — errors");
    // ------------------------------------------------------------------
    {
        bool threw = false;
        try { parse(""); } catch (const ParseError&) { threw = true; }
        CHECK(R, threw);
    }
    {
        bool threw = false;
        try { parse("Every student"); } catch (const ParseError&) { threw = true; }
        CHECK(R, threw);
    }

    // ------------------------------------------------------------------
    R.suite("parse_steps");
    // ------------------------------------------------------------------
    {
        auto results = parse_steps("Every student likes a cat");
        CHECK(R, !results.empty());
        auto& steps = results[0].steps;
        CHECK(R, steps.size() > 1);
        // Final step should be fully reduced (same as parse result)
        auto final_pp = pretty(steps.back());
        CHECK(R, contains(final_pp, "\u2200") && contains(final_pp, "student"));
    }
}
