# lambda.

[![tests](https://github.com/daltonoscar0/lambda/actions/workflows/tests.yml/badge.svg)](https://github.com/daltonoscar0/lambda/actions/workflows/tests.yml)

A from-scratch semantic parser for natural language. It takes an English sentence, builds a Combinatory Categorial Grammar (CCG) derivation, and outputs a first-order logic formula using standard quantifier notation (∀, ∃, ∧, ->, ¬).

## What is CCG?

Combinatory Categorial Grammar is a grammar formalism where every word is assigned a syntactic **category** that encodes what it needs from its neighbours to form a larger constituent. A transitive verb like *likes* carries the category `(S\NP)/NP`, meaning: "give me an NP to my right (the object), then an NP to my left (the subject), and I'll produce a sentence S." Categories combine via a small set of rules, forward/backward application and composition, without any separate phrase-structure rules. The grammar is entirely lexical.

Each lexical entry also carries a **lambda expression** that captures its meaning. When two categories combine, their lambda expressions compose via beta-reduction. Chaining these reductions bottom-up over the parse chart produces the logical form for the whole sentence.

## Example sentences

| Input | Logical form |
|-------|-------------|
| Every student likes a cat | `∀x. (student(x) → ∃y. (cat(y) ∧ ∃z. (like(z) ∧ agent(z, x) ∧ patient(z, y))))` |
| Some person sees every dog | `∃x. (person(x) ∧ ∀y. (dog(y) → ∃z. (see(z) ∧ agent(z, x) ∧ patient(z, y))))` |
| No cat likes a dog | `¬∃x. (cat(x) ∧ ∃y. (dog(y) ∧ ∃z. (like(z) ∧ agent(z, x) ∧ patient(z, y))))` |
| A student likes every cat | `∃x. (student(x) ∧ ∀y. (cat(y) → ∃z. (like(z) ∧ agent(z, x) ∧ patient(z, y))))` |
| Every dog runs | `∀x. (dog(x) → ∃y. (run(y) ∧ agent(y, x)))` |

Verbs get a **neo-Davidsonian** treatment: rather than a binary `likes(x, y)`,
a verb introduces an event variable with `agent` and `patient` roles. That is
why quantifier scope is visible in these forms but the predicate is decomposed.

*(Variable names are auto-generated and may differ; the logical structure is identical.)*

## Install & run

No external libraries are required, only a C++17 compiler. The Scalpel
tokenizer is vendored under `vendor/scalpel/`.

```bash
# Build
make

# Parse a single sentence
./build/lambda "Every student likes a cat"

# Interactive REPL
./build/lambda --interactive        # or -i

# Show beta-reduction steps
./build/lambda --show-steps "Some person sees every dog"

# Other flags
./build/lambda --show-tree  "..."   # CCG derivation tree
./build/lambda --json       "..."   # JSON output
./build/lambda --all-parses "..."   # every parse, not just the first

# Batch mode (one sentence per line)
cat examples/sentences.txt | ./build/lambda

# Build and run the tests
make test
```

## How *every* becomes ∀

The word *every* is stored in the lexicon as:

```
Category: NP/N
Meaning:  λP. λQ. ∀x. P(x) → Q(x)
```

This is a **generalized quantifier**: a doubly-curried function. First it takes a noun property `P` (e.g., `λx. student(x)`), then a predicate `Q` (supplied by the VP), and asserts that every thing satisfying P also satisfies Q.

For "Every student":
```
(λP. λQ. ∀x. P(x) → Q(x)) (λx. student(x))
  → λQ. ∀x. student(x) → Q(x)
```

This NP then acts as the subject. The VP "likes a cat" is built to accept such a generalized quantifier and produce the final sentence:
```
(λQ. ∀x. student(x) → Q(x)) (λx. ∃y. cat(y) ∧ ∃z. like(z) ∧ agent(z, x) ∧ patient(z, y))
  → ∀x. (student(x) → ∃y. (cat(y) ∧ ∃z. (like(z) ∧ agent(z, x) ∧ patient(z, y))))
```

The quantifier machinery is unchanged by the event decomposition: `every` is
still `λP. λQ. ∀x. P(x) → Q(x)`, and only the verb's contribution to `Q` differs.

## Extending the lexicon

Open `src/lexicon.cpp` and add entries using the helper functions:

```cpp
// New noun: singular, plural, predicate
add_noun(t, "fox", "foxes", "fox");

// New transitive verb: 3sg, base, predicate
add_tv(t, "admires", "admire", "admire");

// New intransitive verb
add_iv(t, "flies", "fly", "fly");

// Past-tense forms have their own helpers
add_tv_past(t, "admired", "admire");
add_iv_past(t, "flew", "fly");
```

These register both inflected forms in the table, so `add_noun` above makes
*fox* and *foxes* both available as `N`.

Unknown words are automatically treated as nouns, so *"Every wumpus chases a troll"* parses out of the box.

## Architecture

```
src/term.hpp          Term AST (Var, Const, App, Lam, Forall, Exists, And, Implies, Not)
src/lambda_calc.cpp   beta_reduce, substitute, free_vars
src/category.cpp      Category type + parsing/printing of CCG categories
src/ccg_rules.cpp     Forward/backward application and composition rules
src/lexicon.cpp       Lexicon table + lookup()
src/tokenizer.cpp     Bridge onto the vendored Scalpel tokenizer
src/parser.cpp        CYK chart parser — fills chart[i][j] bottom-up
src/pretty_print.cpp  Converts Terms to ∀∃λ unicode strings
main.cpp              CLI entry point (flag parsing + REPL)
vendor/scalpel/       Vendored Scalpel tokenizer sources
tests/test_main.cpp   Test runner; suites in tests/*.hpp
```

## License

MIT. See [LICENSE](LICENSE).
