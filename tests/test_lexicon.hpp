#pragma once
#include "test_runner.hpp"
#include "../src/lexicon.hpp"
#include "../src/lambda_calc.hpp"

inline void test_lexicon(TestRunner& R) {

    R.suite("lexicon_lookup — known words");
    for (auto& w : {"student","cat","person","dog","likes","sees","every","some","a","no"}) {
        auto entries = lexicon_lookup(w);
        CHECK(R, !entries.empty());
    }

    R.suite("lexicon_lookup — noun category");
    for (auto& w : {"student","cat","dog","person","book"}) {
        auto entries = lexicon_lookup(w);
        CHECK(R, !entries.empty() && cat_equal(entries[0].cat, CAT_N));
    }

    R.suite("lexicon_lookup — determiner category");
    { auto det = cat_NPfN();
      for (auto& w : {"every","some","a","an","no","the"}) {
          auto entries = lexicon_lookup(w);
          bool found = false;
          for (auto& e : entries) if (cat_equal(e.cat, det)) { found = true; break; }
          CHECK(R, found);
      }
    }

    R.suite("lexicon_lookup — transitive verb category");
    { auto tv = cat_SbNP_fNP();
      for (auto& w : {"likes","sees","chases"}) {
          auto entries = lexicon_lookup(w);
          bool found = false;
          for (auto& e : entries) if (cat_equal(e.cat, tv)) { found = true; break; }
          CHECK(R, found);
      }
    }

    R.suite("lexicon_lookup — proper names");
    for (auto& w : {"john","mary","alice","bob"}) {
        auto entries = lexicon_lookup(w);
        CHECK(R, !entries.empty() && cat_equal(entries[0].cat, CAT_NP));
    }

    R.suite("lexicon_lookup — case insensitive");
    { auto lo = lexicon_lookup("student");
      auto up = lexicon_lookup("Student");
      CHECK(R, lo.size() == up.size());
      if (!lo.empty() && !up.empty())
          CHECK(R, cat_equal(lo[0].cat, up[0].cat));
    }

    R.suite("lexicon_lookup — unknown word fallback");
    { auto entries = lexicon_lookup("wumpus");
      CHECK(R, entries.size() == 1);
      CHECK(R, cat_equal(entries[0].cat, CAT_N));
    }

    R.suite("lexicon_lookup — determiner lambdas doubly curried");
    for (auto& w : {"every","some","a","no"}) {
        auto entries = lexicon_lookup(w);
        for (auto& e : entries) {
            if (cat_equal(e.cat, cat_NPfN())) {
                auto* lam1 = std::get_if<Lam>(&e.sem->data);
                CHECK(R, lam1 != nullptr);
                if (lam1) {
                    auto* lam2 = std::get_if<Lam>(&lam1->body->data);
                    CHECK(R, lam2 != nullptr);
                }
            }
        }
    }

    R.suite("lexicon_lookup — determiners have no free vars");
    for (auto& w : {"every","some","a","no"}) {
        for (auto& e : lexicon_lookup(w))
            if (cat_equal(e.cat, cat_NPfN()))
                CHECK(R, free_vars(e.sem).empty());
    }
}
