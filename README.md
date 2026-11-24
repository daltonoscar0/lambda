# lambda.

A from-scratch semantic parser for natural language. It takes an English sentence, builds a Combinatory Categorial Grammar (CCG) derivation, and outputs a first-order logic formula using standard quantifier notation (∀, ∃, ∧, ->, ¬).

## What is CCG?

Combinatory Categorial Grammar is a grammar formalism where every word is assigned a syntactic **category** that encodes what it needs from its neighbours to form a larger constituent. A transitive verb like *likes* carries the category `(S\NP)/NP`, meaning: "give me an NP to my right (the object), then an NP to my left (the subject), and I'll produce a sentence S." Categories combine via a small set of rules, forward/backward application and composition, without any separate phrase-structure rules. The grammar is entirely lexical.

Each lexical entry also carries a **lambda expression** that captures its meaning. When two categories combine, their lambda expressions compose via beta-reduction. Chaining these reductions bottom-up over the parse chart produces the logical form for the whole sentence.

## Example sentences

| Input | Logical form |
|-------|-------------|
| Every student likes a cat | `∀x. student(x) → ∃y. cat(y) ∧ likes(x, y)` |
| Some person sees every dog | `∃x. person(x) ∧ ∀y. dog(y) → sees(x, y)` |
| No cat likes a dog | `¬∃x. cat(x) ∧ ∃y. dog(y) ∧ likes(x, y)` |
| A student likes every cat | `∃x. student(x) ∧ ∀y. cat(y) → likes(x, y)` |
| Every dog runs | `∀x. dog(x) → runs(x)` |

*(Variable names are auto-generated and may differ; the logical structure is identical.)*

## Install & run

No external NLP libraries are required, only Python 3.11+.

```bash
# Clone / copy the project, then:
cd "lambda."

# Parse a single sentence
python main.py "Every student likes a cat"

# Interactive REPL
python main.py --interactive

# Show beta-reduction steps
python main.py --show-steps "Some person sees every dog"

# Batch mode (one sentence per line)
cat examples/sentences.txt | python main.py

# Run tests
python -m pytest tests/ -v
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
(λQ. ∀x. student(x) → Q(x)) (λx. ∃y. cat(y) ∧ likes(x, y))
  → ∀x. student(x) → ∃y. cat(y) ∧ likes(x, y)
```

## Extending the lexicon

Open `src/lexicon.py` and add entries using the helper functions:

```python
# New noun
"fox": [(N, _noun("fox"))],

# New transitive verb
"admires": [(_SbNP_fNP, _trans("admires"))],

# New intransitive verb
"flies": [(_SbNP, _intrans("flies"))],

# New proper name
"eve": [(NP, _proper("eve"))],
```

Unknown words are automatically treated as nouns, so *"Every wumpus chases a troll"* parses out of the box.

## Architecture

```
src/lambda_calc.py   Term AST (Var, Const, App, Lam, Forall, Exists, And, Implies, Not)
                     + beta_reduce, substitute, free_vars
src/ccg_types.py     Category classes + forward/backward application & composition rules
src/lexicon.py       Lexicon table + lookup()
src/parser.py        CYK chart parser — fills chart[i][j] bottom-up
src/pretty_print.py  Converts Terms to ∀∃λ unicode strings
main.py              CLI entry point (argparse + REPL)
```
