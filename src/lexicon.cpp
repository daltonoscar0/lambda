#include "lexicon.hpp"
#include "lambda_calc.hpp"
#include <algorithm>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Additional category helpers (local to this file)
// ---------------------------------------------------------------------------
static Category cat_NfN()    { return make_forward(CAT_N, CAT_N); }
static Category cat_VPbVP()  { return make_backward(cat_SbNP(), cat_SbNP()); }
static Category cat_VPfVP()  { return make_forward(cat_SbNP(), cat_SbNP()); }
static Category cat_DTV()    { return make_forward(cat_SbNP_fNP(), CAT_NP); }
static Category cat_PPmod()  { return make_forward(cat_VPbVP(), CAT_NP); }
static Category cat_NNmod()  { return make_forward(make_backward(CAT_N, CAT_N), CAT_NP); }
// Coordination categories
static Category cat_NP_conj(){ return make_forward(make_backward(CAT_NP, CAT_NP), CAT_NP); }
static Category cat_VP_conj(){ return make_forward(cat_VPbVP(), cat_SbNP()); }
static Category cat_N_conj() { return make_forward(make_backward(CAT_N, CAT_N), CAT_N); }
static Category cat_S_conj() { return make_forward(make_backward(CAT_S, CAT_S), CAT_S); }
// Relative clause categories
static Category cat_relS()   { return make_forward(make_backward(CAT_N,CAT_N), cat_SbNP()); }
static Category cat_relO()   { return make_forward(make_backward(CAT_N,CAT_N), make_forward(CAT_S,CAT_NP)); }
// Question categories
static Category cat_QfVP()   { return make_forward(CAT_Q, cat_SbNP()); }

// ---------------------------------------------------------------------------
// Semantic building blocks
// ---------------------------------------------------------------------------

// N: λx. pred(x)
static Term t_noun(const std::string& pred) {
    return make_lam("x", make_app(make_const(pred), make_var("x")));
}

// NP/N (every / each / all / any / both / most): λP. λQ. ∀x. P(x) -> Q(x)
static Term t_det_every() {
    return make_lam("P", make_lam("Q",
               make_forall("x",
                   make_implies(make_app(make_var("P"), make_var("x")),
                                make_app(make_var("Q"), make_var("x"))))));
}

// NP/N (some / a / an / many / few): λP. λQ. ∃x. P(x) ∧ Q(x)
static Term t_det_some() {
    return make_lam("P", make_lam("Q",
               make_exists("x",
                   make_and(make_app(make_var("P"), make_var("x")),
                            make_app(make_var("Q"), make_var("x"))))));
}

// NP/N (the / this / that / these / those): λP. λQ. Q(ιx. P(x))
static Term t_det_iota() {
    auto Px    = make_app(make_var("P"), make_var("x"));
    auto iota  = make_iota("x", Px);
    auto Qiota = make_app(make_var("Q"), iota);
    return make_lam("P", make_lam("Q", Qiota));
}

// NP/N (no / neither): λP. λQ. ¬∃x. P(x) ∧ Q(x)
static Term t_det_no() {
    return make_lam("P", make_lam("Q",
               make_not(make_exists("x",
                   make_and(make_app(make_var("P"), make_var("x")),
                            make_app(make_var("Q"), make_var("x")))))));
}

// NP (proper name / wh-word, GQ-lifted): λP. P(name)
static Term t_proper(const std::string& name) {
    return make_lam("P", make_app(make_var("P"), make_const(name)));
}

// S\NP (intransitive VP, neo-Davidsonian): λS. S(λx. ∃e. pred(e) ∧ agent(e,x))
static Term t_intrans(const std::string& pred) {
    auto Ve  = make_app(make_const(pred), make_var("e"));
    auto ag  = make_app(make_app(make_const("agent"), make_var("e")), make_var("x"));
    auto ex  = make_exists("e", make_and(Ve, ag));
    return make_lam("S", make_app(make_var("S"), make_lam("x", ex)));
}

// (S\NP)/NP (transitive VP, neo-Davidsonian):
// λO. λS. S(λx. O(λy. ∃e. pred(e) ∧ agent(e,x) ∧ patient(e,y)))
static Term t_trans(const std::string& pred) {
    auto Ve   = make_app(make_const(pred), make_var("e"));
    auto ag   = make_app(make_app(make_const("agent"),   make_var("e")), make_var("x"));
    auto pt   = make_app(make_app(make_const("patient"),  make_var("e")), make_var("y"));
    auto body = make_exists("e", make_and(Ve, make_and(ag, pt)));
    auto lam_y  = make_lam("y",  body);
    auto app_O  = make_app(make_var("O"), lam_y);
    auto lam_x  = make_lam("x",  app_O);
    auto app_S  = make_app(make_var("S"), lam_x);
    return make_lam("O", make_lam("S", app_S));
}

// ((S\NP)/NP)/NP (ditransitive, neo-Davidsonian):
// λIO. λDO. λS. S(λx. IO(λy. DO(λz. ∃e. pred(e) ∧ agent(e,x) ∧ recipient(e,y) ∧ patient(e,z))))
static Term t_ditrans(const std::string& pred) {
    auto Ve   = make_app(make_const(pred), make_var("e"));
    auto ag   = make_app(make_app(make_const("agent"),     make_var("e")), make_var("x"));
    auto rec  = make_app(make_app(make_const("recipient"), make_var("e")), make_var("y"));
    auto pt   = make_app(make_app(make_const("patient"),   make_var("e")), make_var("z"));
    auto body = make_exists("e", make_and(Ve, make_and(ag, make_and(rec, pt))));
    auto lam_z  = make_lam("z",  body);
    auto app_DO = make_app(make_var("DO"), lam_z);
    auto lam_y  = make_lam("y",  app_DO);
    auto app_IO = make_app(make_var("IO"), lam_y);
    auto lam_x  = make_lam("x",  app_IO);
    auto app_S  = make_app(make_var("S"),  lam_x);
    return make_lam("IO", make_lam("DO", make_lam("S", app_S)));
}

// N/N (adjective): λP. λx. adj(x) ∧ P(x)
static Term t_adj(const std::string& pred) {
    auto pred_x = make_app(make_const(pred), make_var("x"));
    auto Px     = make_app(make_var("P"), make_var("x"));
    return make_lam("P", make_lam("x", make_and(pred_x, Px)));
}

// (S\NP)\(S\NP) (VP adverb): λV. λS. V(λf. S(λx. adv(x) ∧ f(x)))
static Term t_vpadv(const std::string& pred) {
    auto adv_x = make_app(make_const(pred), make_var("x"));
    auto fx    = make_app(make_var("f"), make_var("x"));
    auto lam_x = make_lam("x", make_and(adv_x, fx));
    auto app_S = make_app(make_var("S"), lam_x);
    auto app_V = make_app(make_var("V"), make_lam("f", app_S));
    return make_lam("V", make_lam("S", app_V));
}

// (S\NP)/(S\NP) (negation/auxiliary): λV. λS. S(λx. ¬V(λP.P(x)))
static Term t_neg() {
    auto Px    = make_app(make_var("P"), make_var("x"));
    auto V_lam = make_app(make_var("V"), make_lam("P", Px));
    return make_lam("V", make_lam("S",
        make_app(make_var("S"), make_lam("x", make_not(V_lam)))));
}

// (S\NP)/(S\NP) (modal): λV. λS. S(λx. modal(V(λP.P(x))))
static Term t_modal(const std::string& modal_const) {
    auto Px      = make_app(make_var("P"), make_var("x"));
    auto V_lam   = make_app(make_var("V"), make_lam("P", Px));
    auto mod_app = make_app(make_const(modal_const), V_lam);
    return make_lam("V", make_lam("S",
        make_app(make_var("S"), make_lam("x", mod_app))));
}

// ((S\NP)\(S\NP))/NP (prep VP adjunct):
// λO. λV. λS. V(λf. S(λx. f(x) ∧ O(λy. prep(x,y))))
static Term t_prep_vp(const std::string& pred) {
    auto prep_xy = make_app(make_app(make_const(pred), make_var("x")), make_var("y"));
    auto app_O   = make_app(make_var("O"), make_lam("y", prep_xy));
    auto conj    = make_and(make_app(make_var("f"), make_var("x")), app_O);
    auto lam_x   = make_lam("x", conj);
    auto app_S   = make_app(make_var("S"), lam_x);
    auto app_V   = make_app(make_var("V"), make_lam("f", app_S));
    return make_lam("O", make_lam("V", make_lam("S", app_V)));
}

// (N\N)/NP (prep N post-modifier): λO. λN. λx. N(x) ∧ O(λy. prep(x,y))
static Term t_prep_n(const std::string& pred) {
    auto prep_xy = make_app(make_app(make_const(pred), make_var("x")), make_var("y"));
    auto app_O   = make_app(make_var("O"), make_lam("y", prep_xy));
    auto Nx      = make_app(make_var("N"), make_var("x"));
    return make_lam("O", make_lam("N", make_lam("x", make_and(Nx, app_O))));
}

// Copula semantics
// (S\NP)/NP: λO. λS. S(λx. O(λy. eq(x,y)))
static Term t_cop_eq() {
    auto eq_xy = make_app(make_app(make_const("eq"), make_var("x")), make_var("y"));
    auto lam_y = make_lam("y", eq_xy);
    auto app_O = make_app(make_var("O"), lam_y);
    return make_lam("O", make_lam("S",
        make_app(make_var("S"), make_lam("x", app_O))));
}

// (S\NP)/(S\NP): identity, copula for predicative adjectives "John is brave"
static Term t_cop_id() {
    return make_lam("V", make_lam("S", make_app(make_var("V"), make_var("S"))));
}

// Coordination, AND
// (NP\NP)/NP: λB. λA. λP. A(P) ∧ B(P)
static Term t_conj_and_gq() {
    auto AP = make_app(make_var("A"), make_var("P"));
    auto BP = make_app(make_var("B"), make_var("P"));
    return make_lam("B", make_lam("A", make_lam("P", make_and(AP, BP))));
}
// ((S\NP)\(S\NP))/(S\NP): λB. λA. λS. A(S) ∧ B(S)
static Term t_conj_and_vp() {
    auto AS_ = make_app(make_var("A"), make_var("S"));
    auto BS_ = make_app(make_var("B"), make_var("S"));
    return make_lam("B", make_lam("A", make_lam("S", make_and(AS_, BS_))));
}
// (N\N)/N: λB. λA. λx. A(x) ∧ B(x)
static Term t_conj_and_n() {
    auto Ax = make_app(make_var("A"), make_var("x"));
    auto Bx = make_app(make_var("B"), make_var("x"));
    return make_lam("B", make_lam("A", make_lam("x", make_and(Ax, Bx))));
}
// (S\S)/S: λB. λA. A ∧ B
static Term t_conj_and_s() {
    return make_lam("B", make_lam("A", make_and(make_var("A"), make_var("B"))));
}

// Coordination, OR
static Term t_conj_or_gq() {
    auto AP = make_app(make_var("A"), make_var("P"));
    auto BP = make_app(make_var("B"), make_var("P"));
    return make_lam("B", make_lam("A", make_lam("P", make_or(AP, BP))));
}
static Term t_conj_or_vp() {
    auto AS_ = make_app(make_var("A"), make_var("S"));
    auto BS_ = make_app(make_var("B"), make_var("S"));
    return make_lam("B", make_lam("A", make_lam("S", make_or(AS_, BS_))));
}
static Term t_conj_or_n() {
    auto Ax = make_app(make_var("A"), make_var("x"));
    auto Bx = make_app(make_var("B"), make_var("x"));
    return make_lam("B", make_lam("A", make_lam("x", make_or(Ax, Bx))));
}
static Term t_conj_or_s() {
    return make_lam("B", make_lam("A", make_or(make_var("A"), make_var("B"))));
}

// Relative clauses
// (N\N)/(S\NP): λV. λN. λx. N(x) ∧ V(λP.P(x))
static Term t_rel_subj() {
    auto Px    = make_app(make_var("P"), make_var("x"));
    auto V_lam = make_app(make_var("V"), make_lam("P", Px));
    auto Nx    = make_app(make_var("N"), make_var("x"));
    return make_lam("V", make_lam("N", make_lam("x", make_and(Nx, V_lam))));
}
// (N\N)/(S/NP): λG. λN. λx. N(x) ∧ G(λP.P(x))
static Term t_rel_obj() {
    auto Px    = make_app(make_var("P"), make_var("x"));
    auto G_lam = make_app(make_var("G"), make_lam("P", Px));
    auto Nx    = make_app(make_var("N"), make_var("x"));
    return make_lam("G", make_lam("N", make_lam("x", make_and(Nx, G_lam))));
}

// ---------------------------------------------------------------------------
// Lexicon table helpers
// ---------------------------------------------------------------------------
using Table = std::unordered_map<std::string, std::vector<LexEntry>>;

static void add_noun(Table& t, const std::string& sg, const std::string& pl,
                     const std::string& pred) {
    t[sg] = {{ CAT_N, t_noun(pred) }};
    t[pl] = {{ CAT_N, t_noun(pred) }};
}

static void add_iv(Table& t, const std::string& sg3, const std::string& base,
                   const std::string& pred) {
    t[sg3]  = {{ cat_SbNP(), t_intrans(pred) }};
    t[base] = {{ cat_SbNP(), t_intrans(pred) }};
}

static void add_iv_past(Table& t, const std::string& past, const std::string& pred) {
    t[past] = {{ cat_SbNP(), t_intrans(pred) }};
}

static void add_tv(Table& t, const std::string& sg3, const std::string& base,
                   const std::string& pred) {
    t[sg3]  = {{ cat_SbNP_fNP(), t_trans(pred) }};
    t[base] = {{ cat_SbNP_fNP(), t_trans(pred) }};
}

static void add_tv_past(Table& t, const std::string& past, const std::string& pred) {
    t[past] = {{ cat_SbNP_fNP(), t_trans(pred) }};
}

static void add_dtv(Table& t, const std::string& sg3, const std::string& base,
                    const std::string& pred) {
    t[sg3]  = {{ cat_DTV(), t_ditrans(pred) }};
    t[base] = {{ cat_DTV(), t_ditrans(pred) }};
}

static void add_dtv_past(Table& t, const std::string& past, const std::string& pred) {
    t[past] = {{ cat_DTV(), t_ditrans(pred) }};
}

static void add_adj(Table& t, const std::string& w) {
    // Both prenominal (N/N) and predicative (S\NP) entries
    t[w] = {{ cat_NfN(), t_adj(w) }, { cat_SbNP(), t_intrans(w) }};
}

static void add_prep(Table& t, const std::string& w, const std::string& pred) {
    t[w] = {
        { cat_PPmod(), t_prep_vp(pred) },
        { cat_NNmod(), t_prep_n(pred)  }
    };
}

static Table build_table() {
    const auto NPfN = cat_NPfN();
    const auto SbNP = cat_SbNP();
    const auto TV   = cat_SbNP_fNP();
    const auto VPfV = cat_VPfVP();
    const auto VPbV = cat_VPbVP();
    const auto Q_VP = cat_QfVP();

    Table t;

    // -----------------------------------------------------------------------
    // Nouns, social roles & professions
    // -----------------------------------------------------------------------
    add_noun(t, "student",       "students",       "student");
    add_noun(t, "teacher",       "teachers",       "teacher");
    add_noun(t, "doctor",        "doctors",        "doctor");
    add_noun(t, "lawyer",        "lawyers",        "lawyer");
    add_noun(t, "scientist",     "scientists",     "scientist");
    add_noun(t, "artist",        "artists",        "artist");
    add_noun(t, "athlete",       "athletes",       "athlete");
    add_noun(t, "politician",    "politicians",    "politician");
    add_noun(t, "professor",     "professors",     "professor");
    add_noun(t, "nurse",         "nurses",         "nurse");
    add_noun(t, "engineer",      "engineers",      "engineer");
    add_noun(t, "writer",        "writers",        "writer");
    add_noun(t, "musician",      "musicians",      "musician");
    add_noun(t, "chef",          "chefs",          "chef");
    add_noun(t, "soldier",       "soldiers",       "soldier");
    add_noun(t, "priest",        "priests",        "priest");
    add_noun(t, "judge",         "judges",         "judge");
    add_noun(t, "philosopher",   "philosophers",   "philosopher");
    add_noun(t, "mathematician", "mathematicians", "mathematician");
    add_noun(t, "linguist",      "linguists",      "linguist");
    add_noun(t, "poet",          "poets",          "poet");
    add_noun(t, "painter",       "painters",       "painter");
    add_noun(t, "architect",     "architects",     "architect");
    add_noun(t, "surgeon",       "surgeons",       "surgeon");
    add_noun(t, "pilot",         "pilots",         "pilot");
    add_noun(t, "astronaut",     "astronauts",     "astronaut");
    add_noun(t, "farmer",        "farmers",        "farmer");
    add_noun(t, "merchant",      "merchants",      "merchant");
    add_noun(t, "servant",       "servants",       "servant");
    add_noun(t, "guard",         "guards",         "guard");
    add_noun(t, "clerk",         "clerks",         "clerk");
    add_noun(t, "hunter",        "hunters",        "hunter");
    add_noun(t, "sailor",        "sailors",        "sailor");
    add_noun(t, "officer",       "officers",       "officer");
    add_noun(t, "detective",     "detectives",     "detective");
    add_noun(t, "reporter",      "reporters",      "reporter");
    add_noun(t, "editor",        "editors",        "editor");
    add_noun(t, "director",      "directors",      "director");
    add_noun(t, "manager",       "managers",       "manager");
    add_noun(t, "leader",        "leaders",        "leader");
    add_noun(t, "master",        "masters",        "master");
    add_noun(t, "minister",      "ministers",      "minister");
    add_noun(t, "bishop",        "bishops",        "bishop");
    add_noun(t, "captain",       "captains",       "captain");
    add_noun(t, "general",       "generals",       "general");
    add_noun(t, "president",     "presidents",     "president");
    add_noun(t, "emperor",       "emperors",       "emperor");
    add_noun(t, "champion",      "champions",      "champion");
    add_noun(t, "beggar",        "beggars",        "beggar");
    add_noun(t, "slave",         "slaves",         "slave");
    add_noun(t, "prisoner",      "prisoners",      "prisoner");
    add_noun(t, "spy",           "spies",          "spy");
    add_noun(t, "agent",         "agents",         "agent");
    add_noun(t, "ambassador",    "ambassadors",    "ambassador");
    add_noun(t, "hero",          "heroes",         "hero");
    add_noun(t, "villain",       "villains",       "villain");
    add_noun(t, "monster",       "monsters",       "monster");
    add_noun(t, "ghost",         "ghosts",         "ghost");
    add_noun(t, "angel",         "angels",         "angel");
    add_noun(t, "demon",         "demons",         "demon");
    add_noun(t, "saint",         "saints",         "saint");
    add_noun(t, "prophet",       "prophets",       "prophet");
    add_noun(t, "wizard",        "wizards",        "wizard");
    add_noun(t, "knight",        "knights",        "knight");
    add_noun(t, "king",          "kings",          "king");
    add_noun(t, "queen",         "queens",         "queen");
    add_noun(t, "prince",        "princes",        "prince");
    add_noun(t, "princess",      "princesses",     "princess");
    add_noun(t, "duke",          "dukes",          "duke");
    add_noun(t, "person",        "people",         "person");
    add_noun(t, "child",         "children",       "child");
    add_noun(t, "man",           "men",            "man");
    add_noun(t, "woman",         "women",          "woman");
    add_noun(t, "friend",        "friends",        "friend");
    add_noun(t, "enemy",         "enemies",        "enemy");
    add_noun(t, "stranger",      "strangers",      "stranger");
    add_noun(t, "neighbour",     "neighbours",     "neighbour");
    add_noun(t, "neighbor",      "neighbors",      "neighbor");
    add_noun(t, "citizen",       "citizens",       "citizen");

    // -----------------------------------------------------------------------
    // Nouns, animals
    // -----------------------------------------------------------------------
    add_noun(t, "cat",      "cats",      "cat");
    add_noun(t, "dog",      "dogs",      "dog");
    add_noun(t, "bird",     "birds",     "bird");
    add_noun(t, "fish",     "fish",      "fish");
    add_noun(t, "horse",    "horses",    "horse");
    add_noun(t, "cow",      "cows",      "cow");
    add_noun(t, "lion",     "lions",     "lion");
    add_noun(t, "tiger",    "tigers",    "tiger");
    add_noun(t, "bear",     "bears",     "bear");
    add_noun(t, "wolf",     "wolves",    "wolf");
    add_noun(t, "fox",      "foxes",     "fox");
    add_noun(t, "rabbit",   "rabbits",   "rabbit");
    add_noun(t, "snake",    "snakes",    "snake");
    add_noun(t, "eagle",    "eagles",    "eagle");
    add_noun(t, "owl",      "owls",      "owl");
    add_noun(t, "dolphin",  "dolphins",  "dolphin");
    add_noun(t, "whale",    "whales",    "whale");
    add_noun(t, "shark",    "sharks",    "shark");
    add_noun(t, "elephant", "elephants", "elephant");
    add_noun(t, "giraffe",  "giraffes",  "giraffe");
    add_noun(t, "monkey",   "monkeys",   "monkey");
    add_noun(t, "deer",     "deer",      "deer");
    add_noun(t, "mouse",    "mice",      "mouse");
    add_noun(t, "rat",      "rats",      "rat");
    add_noun(t, "bee",      "bees",      "bee");
    add_noun(t, "ant",      "ants",      "ant");
    add_noun(t, "spider",   "spiders",   "spider");
    add_noun(t, "frog",     "frogs",     "frog");
    add_noun(t, "duck",     "ducks",     "duck");
    add_noun(t, "swan",     "swans",     "swan");
    add_noun(t, "crow",     "crows",     "crow");
    add_noun(t, "hawk",     "hawks",     "hawk");
    add_noun(t, "bat",      "bats",      "bat");
    add_noun(t, "dragon",   "dragons",   "dragon");

    // -----------------------------------------------------------------------
    // Nouns, objects & places
    // -----------------------------------------------------------------------
    add_noun(t, "book",       "books",       "book");
    add_noun(t, "table",      "tables",      "table");
    add_noun(t, "chair",      "chairs",      "chair");
    add_noun(t, "door",       "doors",       "door");
    add_noun(t, "window",     "windows",     "window");
    add_noun(t, "house",      "houses",      "house");
    add_noun(t, "room",       "rooms",       "room");
    add_noun(t, "car",        "cars",        "car");
    add_noun(t, "ship",       "ships",       "ship");
    add_noun(t, "boat",       "boats",       "boat");
    add_noun(t, "plane",      "planes",      "plane");
    add_noun(t, "train",      "trains",      "train");
    add_noun(t, "road",       "roads",       "road");
    add_noun(t, "river",      "rivers",      "river");
    add_noun(t, "mountain",   "mountains",   "mountain");
    add_noun(t, "tree",       "trees",       "tree");
    add_noun(t, "flower",     "flowers",     "flower");
    add_noun(t, "stone",      "stones",      "stone");
    add_noun(t, "rock",       "rocks",       "rock");
    add_noun(t, "cloud",      "clouds",      "cloud");
    add_noun(t, "star",       "stars",       "star");
    add_noun(t, "world",      "worlds",      "world");
    add_noun(t, "city",       "cities",      "city");
    add_noun(t, "town",       "towns",       "town");
    add_noun(t, "village",    "villages",    "village");
    add_noun(t, "garden",     "gardens",     "garden");
    add_noun(t, "park",       "parks",       "park");
    add_noun(t, "forest",     "forests",     "forest");
    add_noun(t, "island",     "islands",     "island");
    add_noun(t, "lake",       "lakes",       "lake");
    add_noun(t, "field",      "fields",      "field");
    add_noun(t, "hill",       "hills",       "hill");
    add_noun(t, "valley",     "valleys",     "valley");
    add_noun(t, "bridge",     "bridges",     "bridge");
    add_noun(t, "tower",      "towers",      "tower");
    add_noun(t, "castle",     "castles",     "castle");
    add_noun(t, "church",     "churches",    "church");
    add_noun(t, "school",     "schools",     "school");
    add_noun(t, "university", "universities","university");
    add_noun(t, "hospital",   "hospitals",   "hospital");
    add_noun(t, "prison",     "prisons",     "prison");
    add_noun(t, "market",     "markets",     "market");
    add_noun(t, "shop",       "shops",       "shop");
    add_noun(t, "library",    "libraries",   "library");
    add_noun(t, "museum",     "museums",     "museum");
    add_noun(t, "palace",     "palaces",     "palace");
    add_noun(t, "farm",       "farms",       "farm");
    add_noun(t, "temple",     "temples",     "temple");
    add_noun(t, "throne",     "thrones",     "throne");
    add_noun(t, "crown",      "crowns",      "crown");
    add_noun(t, "sword",      "swords",      "sword");
    add_noun(t, "shield",     "shields",     "shield");
    add_noun(t, "bow",        "bows",        "bow");
    add_noun(t, "arrow",      "arrows",      "arrow");
    add_noun(t, "ring",       "rings",       "ring");
    add_noun(t, "coin",       "coins",       "coin");
    add_noun(t, "gem",        "gems",        "gem");
    add_noun(t, "key",        "keys",        "key");
    add_noun(t, "cage",       "cages",       "cage");
    add_noun(t, "rope",       "ropes",       "rope");
    add_noun(t, "chain",      "chains",      "chain");
    add_noun(t, "wall",       "walls",       "wall");
    add_noun(t, "gate",       "gates",       "gate");
    add_noun(t, "path",       "paths",       "path");
    add_noun(t, "land",       "lands",       "land");
    add_noun(t, "map",        "maps",        "map");
    add_noun(t, "letter",     "letters",     "letter");
    add_noun(t, "message",    "messages",    "message");
    add_noun(t, "mirror",     "mirrors",     "mirror");
    add_noun(t, "cup",        "cups",        "cup");
    add_noun(t, "box",        "boxes",       "box");
    add_noun(t, "bag",        "bags",        "bag");
    add_noun(t, "glass",      "glasses",     "glass");
    add_noun(t, "bottle",     "bottles",     "bottle");
    add_noun(t, "lamp",       "lamps",       "lamp");
    add_noun(t, "bed",        "beds",        "bed");
    add_noun(t, "clock",      "clocks",      "clock");
    add_noun(t, "flag",       "flags",       "flag");
    add_noun(t, "net",        "nets",        "net");
    add_noun(t, "trap",       "traps",       "trap");
    add_noun(t, "office",     "offices",     "office");
    add_noun(t, "street",     "streets",     "street");

    // -----------------------------------------------------------------------
    // Nouns, abstract concepts
    // -----------------------------------------------------------------------
    add_noun(t, "idea",       "ideas",       "idea");
    add_noun(t, "thought",    "thoughts",    "thought");
    add_noun(t, "dream",      "dreams",      "dream");
    add_noun(t, "hope",       "hopes",       "hope");
    add_noun(t, "fear",       "fears",       "fear");
    add_noun(t, "love",       "loves",       "love");
    add_noun(t, "hate",       "hates",       "hate");
    add_noun(t, "truth",      "truths",      "truth");
    add_noun(t, "lie",        "lies",        "lie");
    add_noun(t, "fact",       "facts",       "fact");
    add_noun(t, "theory",     "theories",    "theory");
    add_noun(t, "law",        "laws",        "law");
    add_noun(t, "rule",       "rules",       "rule");
    add_noun(t, "question",   "questions",   "question");
    add_noun(t, "answer",     "answers",     "answer");
    add_noun(t, "problem",    "problems",    "problem");
    add_noun(t, "solution",   "solutions",   "solution");
    add_noun(t, "story",      "stories",     "story");
    add_noun(t, "poem",       "poems",       "poem");
    add_noun(t, "song",       "songs",       "song");
    add_noun(t, "picture",    "pictures",    "picture");
    add_noun(t, "word",       "words",       "word");
    add_noun(t, "name",       "names",       "name");
    add_noun(t, "plan",       "plans",       "plan");
    add_noun(t, "order",      "orders",      "order");
    add_noun(t, "promise",    "promises",    "promise");
    add_noun(t, "wish",       "wishes",      "wish");
    add_noun(t, "belief",     "beliefs",     "belief");
    add_noun(t, "desire",     "desires",     "desire");
    add_noun(t, "right",      "rights",      "right");
    add_noun(t, "power",      "powers",      "power");
    add_noun(t, "duty",       "duties",      "duty");
    add_noun(t, "honour",     "honours",     "honour");
    add_noun(t, "secret",     "secrets",     "secret");
    add_noun(t, "mystery",    "mysteries",   "mystery");
    add_noun(t, "danger",     "dangers",     "danger");
    add_noun(t, "chance",     "chances",     "chance");
    add_noun(t, "choice",     "choices",     "choice");
    add_noun(t, "mistake",    "mistakes",    "mistake");
    add_noun(t, "victory",    "victories",   "victory");
    add_noun(t, "defeat",     "defeats",     "defeat");
    add_noun(t, "journey",    "journeys",    "journey");
    add_noun(t, "adventure",  "adventures",  "adventure");
    add_noun(t, "battle",     "battles",     "battle");
    add_noun(t, "war",        "wars",        "war");
    add_noun(t, "peace",      "peaces",      "peace");
    add_noun(t, "crime",      "crimes",      "crime");
    add_noun(t, "sin",        "sins",        "sin");
    add_noun(t, "reward",     "rewards",     "reward");
    add_noun(t, "punishment", "punishments", "punishment");
    add_noun(t, "gift",       "gifts",       "gift");
    add_noun(t, "lesson",     "lessons",     "lesson");
    add_noun(t, "language",   "languages",   "language");
    add_noun(t, "number",     "numbers",     "number");
    add_noun(t, "sign",       "signs",       "sign");
    add_noun(t, "cause",      "causes",      "cause");
    add_noun(t, "effect",     "effects",     "effect");
    add_noun(t, "purpose",    "purposes",    "purpose");
    add_noun(t, "meaning",    "meanings",    "meaning");
    add_noun(t, "event",      "events",      "event");
    add_noun(t, "moment",     "moments",     "moment");
    add_noun(t, "age",        "ages",        "age");
    add_noun(t, "end",        "ends",        "end");
    add_noun(t, "beginning",  "beginnings",  "beginning");

    // -----------------------------------------------------------------------
    // Nouns, natural / body
    // -----------------------------------------------------------------------
    add_noun(t, "fire",    "fires",    "fire");
    add_noun(t, "wind",    "winds",    "wind");
    add_noun(t, "rain",    "rains",    "rain");
    add_noun(t, "snow",    "snows",    "snow");
    add_noun(t, "storm",   "storms",   "storm");
    add_noun(t, "shadow",  "shadows",  "shadow");
    add_noun(t, "light",   "lights",   "light");
    add_noun(t, "wave",    "waves",    "wave");
    add_noun(t, "moon",    "moons",    "moon");
    add_noun(t, "sun",     "suns",     "sun");
    add_noun(t, "earth",   "earths",   "earth");
    add_noun(t, "ocean",   "oceans",   "ocean");
    add_noun(t, "sea",     "seas",     "sea");
    add_noun(t, "hand",    "hands",    "hand");
    add_noun(t, "eye",     "eyes",     "eye");
    add_noun(t, "ear",     "ears",     "ear");
    add_noun(t, "mouth",   "mouths",   "mouth");
    add_noun(t, "head",    "heads",    "head");
    add_noun(t, "heart",   "hearts",   "heart");
    add_noun(t, "mind",    "minds",    "mind");
    add_noun(t, "face",    "faces",    "face");
    add_noun(t, "arm",     "arms",     "arm");
    add_noun(t, "leg",     "legs",     "leg");
    add_noun(t, "foot",    "feet",     "foot");
    add_noun(t, "back",    "backs",    "back");
    add_noun(t, "voice",   "voices",   "voice");
    add_noun(t, "blood",   "bloods",   "blood");
    add_noun(t, "bone",    "bones",    "bone");

    // -----------------------------------------------------------------------
    // Nouns, food & drink
    // -----------------------------------------------------------------------
    add_noun(t, "apple",  "apples",  "apple");
    add_noun(t, "bread",  "breads",  "bread");
    add_noun(t, "wine",   "wines",   "wine");
    add_noun(t, "milk",   "milks",   "milk");
    add_noun(t, "meat",   "meats",   "meat");
    add_noun(t, "rice",   "rices",   "rice");
    add_noun(t, "soup",   "soups",   "soup");
    add_noun(t, "cake",   "cakes",   "cake");
    add_noun(t, "fruit",  "fruits",  "fruit");
    add_noun(t, "herb",   "herbs",   "herb");

    // -----------------------------------------------------------------------
    // Determiners
    // -----------------------------------------------------------------------
    t["every"]   = {{ NPfN, t_det_every() }};
    t["each"]    = {{ NPfN, t_det_every() }};
    t["all"]     = {{ NPfN, t_det_every() }};
    t["any"]     = {{ NPfN, t_det_every() }};
    t["both"]    = {{ NPfN, t_det_every() }};
    t["most"]    = {{ NPfN, t_det_every() }};
    t["some"]    = {{ NPfN, t_det_some() }};
    t["a"]       = {{ NPfN, t_det_some() }};
    t["an"]      = {{ NPfN, t_det_some() }};
    t["many"]    = {{ NPfN, t_det_some() }};
    t["few"]     = {{ NPfN, t_det_some() }};
    t["another"] = {{ NPfN, t_det_some() }};
    t["other"]   = {{ NPfN, t_det_some() }};
    t["either"]  = {{ NPfN, t_det_some() }};
    t["the"]     = {{ NPfN, t_det_iota() }};
    t["this"]    = {{ NPfN, t_det_iota() }};
    t["these"]   = {{ NPfN, t_det_some() }};  // plural, skip iota
    t["those"]   = {{ NPfN, t_det_some() }};
    t["no"]      = {{ NPfN, t_det_no() }};
    t["neither"] = {{ NPfN, t_det_no() }};
    // "that" as demonstrative det + relative pronoun
    t["that"]    = {
        { NPfN,        t_det_iota()  },
        { cat_relS(),  t_rel_subj()  },
        { cat_relO(),  t_rel_obj()   }
    };

    // -----------------------------------------------------------------------
    // Relative pronouns (who/which are purely relational)
    // -----------------------------------------------------------------------
    t["who"]   = {{ cat_relS(), t_rel_subj() }, { cat_relO(), t_rel_obj() }};
    t["whom"]  = {{ cat_relO(), t_rel_obj() }};
    t["which"] = {{ cat_relS(), t_rel_subj() }, { cat_relO(), t_rel_obj() }};

    // -----------------------------------------------------------------------
    // Wh-words (questions)
    // -----------------------------------------------------------------------
    t["who"]   .push_back({ Q_VP, t_proper("who")  });
    t["what"]  = {{ Q_VP, t_proper("what") }};
    t["which"] .push_back({ Q_VP, t_proper("which") });

    // -----------------------------------------------------------------------
    // Copula
    // -----------------------------------------------------------------------
    {
        auto cop_entries = std::vector<LexEntry>{
            { TV,   t_cop_eq() },   // "is a student"
            { VPfV, t_cop_id() }    // "is brave"
        };
        t["is"]    = cop_entries;
        t["are"]   = cop_entries;
        t["was"]   = cop_entries;
        t["were"]  = cop_entries;
        t["be"]    = cop_entries;
        t["been"]  = cop_entries;
        t["being"] = cop_entries;
        t["am"]    = cop_entries;
    }

    // -----------------------------------------------------------------------
    // Modal verbs  (S\NP)/(S\NP)
    // -----------------------------------------------------------------------
    t["can"]    = {{ VPfV, t_modal("can")    }};
    t["could"]  = {{ VPfV, t_modal("could")  }};
    t["may"]    = {{ VPfV, t_modal("may")    }};
    t["might"]  = {{ VPfV, t_modal("might")  }};
    t["must"]   = {{ VPfV, t_modal("must")   }};
    t["should"] = {{ VPfV, t_modal("should") }};
    t["will"]   = {{ VPfV, t_modal("will")   }};
    t["would"]  = {{ VPfV, t_modal("would")  }};
    t["shall"]  = {{ VPfV, t_modal("shall")  }};
    t["ought"]  = {{ VPfV, t_modal("ought")  }};
    t["need"]   = {{ VPfV, t_modal("need")   }};
    t["dare"]   = {{ VPfV, t_modal("dare")   }};

    // -----------------------------------------------------------------------
    // Negation / auxiliaries  (S\NP)/(S\NP)
    // -----------------------------------------------------------------------
    {
        auto neg = t_neg();
        t["not"]       = {{ VPfV, neg }};
        t["does"]      = {{ VPfV, t_cop_id() }};  // transparent do-support
        t["do"]        = {{ VPfV, t_cop_id() }};
        t["did"]       = {{ VPfV, t_cop_id() }};
        t["doesn't"]   = {{ VPfV, neg }};
        t["don't"]     = {{ VPfV, neg }};
        t["didn't"]    = {{ VPfV, neg }};
        t["won't"]     = {{ VPfV, neg }};
        t["wouldn't"]  = {{ VPfV, neg }};
        t["shouldn't"] = {{ VPfV, neg }};
        t["couldn't"]  = {{ VPfV, neg }};
        t["can't"]     = {{ VPfV, neg }};
        t["isn't"]     = {{ VPfV, neg }};
        t["aren't"]    = {{ VPfV, neg }};
        t["wasn't"]    = {{ VPfV, neg }};
        t["weren't"]   = {{ VPfV, neg }};
        t["cannot"]    = {{ VPfV, neg }};
        t["never"]     = {{ VPfV, neg }, { VPbV, t_vpadv("never") }};
    }

    // -----------------------------------------------------------------------
    // Coordination
    // -----------------------------------------------------------------------
    {
        auto and_np = t_conj_and_gq();
        auto and_vp = t_conj_and_vp();
        auto and_n  = t_conj_and_n();
        auto and_s  = t_conj_and_s();
        t["and"] = {
            { cat_NP_conj(), and_np },
            { cat_VP_conj(), and_vp },
            { cat_N_conj(),  and_n  },
            { cat_S_conj(),  and_s  }
        };
        t["but"] = t["and"];   // same semantics for our purposes
    }
    {
        auto or_np = t_conj_or_gq();
        auto or_vp = t_conj_or_vp();
        auto or_n  = t_conj_or_n();
        auto or_s  = t_conj_or_s();
        t["or"] = {
            { cat_NP_conj(), or_np },
            { cat_VP_conj(), or_vp },
            { cat_N_conj(),  or_n  },
            { cat_S_conj(),  or_s  }
        };
    }

    // -----------------------------------------------------------------------
    // Intransitive verbs
    // -----------------------------------------------------------------------
    add_iv(t, "runs",       "run",       "run");
    add_iv(t, "walks",      "walk",      "walk");
    add_iv(t, "sleeps",     "sleep",     "sleep");
    add_iv(t, "talks",      "talk",      "talk");
    add_iv(t, "works",      "work",      "work");
    add_iv(t, "plays",      "play",      "play");
    add_iv(t, "stands",     "stand",     "stand");
    add_iv(t, "sits",       "sit",       "sit");
    add_iv(t, "falls",      "fall",      "fall");
    add_iv(t, "rises",      "rise",      "rise");
    add_iv(t, "grows",      "grow",      "grow");
    add_iv(t, "moves",      "move",      "move");
    add_iv(t, "stops",      "stop",      "stop");
    add_iv(t, "starts",     "start",     "start");
    add_iv(t, "dies",       "die",       "die");
    add_iv(t, "lives",      "live",      "live");
    add_iv(t, "exists",     "exist",     "exist");
    add_iv(t, "waits",      "wait",      "wait");
    add_iv(t, "smiles",     "smile",     "smile");
    add_iv(t, "laughs",     "laugh",     "laugh");
    add_iv(t, "cries",      "cry",       "cry");
    add_iv(t, "speaks",     "speak",     "speak");
    add_iv(t, "sings",      "sing",      "sing");
    add_iv(t, "dances",     "dance",     "dance");
    add_iv(t, "swims",      "swim",      "swim");
    add_iv(t, "flies",      "fly",       "fly");
    add_iv(t, "jumps",      "jump",      "jump");
    add_iv(t, "climbs",     "climb",     "climb");
    add_iv(t, "arrives",    "arrive",    "arrive");
    add_iv(t, "returns",    "return",    "return");
    add_iv(t, "appears",    "appear",    "appear");
    add_iv(t, "disappears", "disappear", "disappear");
    add_iv(t, "succeeds",   "succeed",   "succeed");
    add_iv(t, "fails",      "fail",      "fail");
    add_iv(t, "wins",       "win",       "win");
    add_iv(t, "loses",      "lose",      "lose");
    add_iv(t, "fights",     "fight",     "fight");
    add_iv(t, "rests",      "rest",      "rest");
    add_iv(t, "hides",      "hide",      "hide");
    add_iv(t, "escapes",    "escape",    "escape");
    add_iv(t, "survives",   "survive",   "survive");
    add_iv(t, "wanders",    "wander",    "wander");
    add_iv(t, "travels",    "travel",    "travel");
    add_iv(t, "thinks",     "think",     "think");
    add_iv(t, "dreams",     "dream",     "dream");
    add_iv(t, "suffers",    "suffer",    "suffer");
    add_iv(t, "struggles",  "struggle",  "struggle");
    add_iv(t, "listens",    "listen",    "listen");
    add_iv(t, "searches",   "search",    "search");
    add_iv(t, "prays",      "pray",      "pray");
    add_iv(t, "shouts",     "shout",     "shout");
    add_iv(t, "whispers",   "whisper",   "whisper");
    add_iv(t, "kneels",     "kneel",     "kneel");
    add_iv(t, "breathes",   "breathe",   "breathe");
    add_iv(t, "shivers",    "shiver",    "shiver");
    add_iv(t, "trembles",   "tremble",   "tremble");
    add_iv(t, "vanishes",   "vanish",    "vanish");
    add_iv(t, "glows",      "glow",      "glow");
    add_iv(t, "shines",     "shine",     "shine");
    add_iv(t, "burns",      "burn",      "burn");
    add_iv(t, "freezes",    "freeze",    "freeze");
    add_iv(t, "melts",      "melt",      "melt");
    add_iv(t, "flows",      "flow",      "flow");
    add_iv(t, "barks",      "bark",      "bark");
    add_iv(t, "meows",      "meow",      "meow");
    add_iv(t, "howls",      "howl",      "howl");
    // "study" is ambi-transitive; set both IV and TV entries here
    // (the add_tv in TV section would overwrite, so we do it last)


    // Past tense, intransitive
    add_iv_past(t, "ran",        "run");
    add_iv_past(t, "fell",       "fall");
    add_iv_past(t, "rose",       "rise");
    add_iv_past(t, "stood",      "stand");
    add_iv_past(t, "sat",        "sit");
    add_iv_past(t, "swam",       "swim");
    add_iv_past(t, "flew",       "fly");
    add_iv_past(t, "sang",       "sing");
    add_iv_past(t, "hid",        "hide");
    add_iv_past(t, "fled",       "flee");
    add_iv_past(t, "fought",     "fight");
    add_iv_past(t, "shone",      "shine");
    add_iv_past(t, "slept",      "sleep");
    add_iv_past(t, "thought",    "think");
    add_iv_past(t, "walked",     "walk");
    add_iv_past(t, "talked",     "talk");
    add_iv_past(t, "worked",     "work");
    add_iv_past(t, "played",     "play");
    add_iv_past(t, "jumped",     "jump");
    add_iv_past(t, "died",       "die");
    add_iv_past(t, "lived",      "live");
    add_iv_past(t, "laughed",    "laugh");
    add_iv_past(t, "smiled",     "smile");
    add_iv_past(t, "cried",      "cry");
    add_iv_past(t, "moved",      "move");
    add_iv_past(t, "danced",     "dance");
    add_iv_past(t, "waited",     "wait");
    add_iv_past(t, "arrived",    "arrive");
    add_iv_past(t, "appeared",   "appear");
    add_iv_past(t, "escaped",    "escape");
    add_iv_past(t, "survived",   "survive");

    // -----------------------------------------------------------------------
    // Transitive verbs
    // -----------------------------------------------------------------------
    add_tv(t, "likes",      "like",      "like");
    add_tv(t, "sees",       "see",       "see");
    add_tv(t, "chases",     "chase",     "chase");
    add_tv(t, "loves",      "love",      "love");
    add_tv(t, "hates",      "hate",      "hate");
    add_tv(t, "knows",      "know",      "know");
    add_tv(t, "reads",      "read",      "read");
    add_tv(t, "feeds",      "feed",      "feed");
    add_tv(t, "helps",      "help",      "help");
    add_tv(t, "finds",      "find",      "find");
    add_tv(t, "makes",      "make",      "make");
    add_tv(t, "takes",      "take",      "take");
    add_tv(t, "gets",       "get",       "get");
    add_tv(t, "puts",       "put",       "put");
    add_tv(t, "keeps",      "keep",      "keep");
    add_tv(t, "holds",      "hold",      "hold");
    add_tv(t, "meets",      "meet",      "meet");
    add_tv(t, "beats",      "beat",      "beat");
    add_tv(t, "kills",      "kill",      "kill");
    add_tv(t, "hits",       "hit",       "hit");
    add_tv(t, "calls",      "call",      "call");
    add_tv(t, "writes",     "write",     "write");
    add_tv(t, "builds",     "build",     "build");
    add_tv(t, "breaks",     "break",     "break");
    add_tv(t, "buys",       "buy",       "buy");
    add_tv(t, "sells",      "sell",      "sell");
    add_tv(t, "brings",     "bring",     "bring");
    add_tv(t, "uses",       "use",       "use");
    add_tv(t, "follows",    "follow",    "follow");
    add_tv(t, "leads",      "lead",      "lead");
    add_tv(t, "joins",      "join",      "join");
    add_tv(t, "seeks",      "seek",      "seek");
    add_tv(t, "wants",      "want",      "want");
    add_tv(t, "needs",      "need",      "need");
    add_tv(t, "owns",       "own",       "own");
    add_tv(t, "deserves",   "deserve",   "deserve");
    add_tv(t, "fears",      "fear",      "fear");
    add_tv(t, "trusts",     "trust",     "trust");
    add_tv(t, "respects",   "respect",   "respect");
    add_tv(t, "teaches",    "teach",     "teach");
    add_tv(t, "changes",    "change",    "change");
    add_tv(t, "controls",   "control",   "control");
    add_tv(t, "defeats",    "defeat",    "defeat");
    add_tv(t, "catches",    "catch",     "catch");
    add_tv(t, "carries",    "carry",     "carry");
    add_tv(t, "throws",     "throw",     "throw");
    add_tv(t, "drops",      "drop",      "drop");
    add_tv(t, "lifts",      "lift",      "lift");
    add_tv(t, "pulls",      "pull",      "pull");
    add_tv(t, "pushes",     "push",      "push");
    add_tv(t, "opens",      "open",      "open");
    add_tv(t, "closes",     "close",     "close");
    add_tv(t, "cuts",       "cut",       "cut");
    add_tv(t, "touches",    "touch",     "touch");
    add_tv(t, "covers",     "cover",     "cover");
    add_tv(t, "includes",   "include",   "include");
    add_tv(t, "contains",   "contain",   "contain");
    add_tv(t, "represents", "represent", "represent");
    add_tv(t, "supports",   "support",   "support");
    add_tv(t, "attacks",    "attack",    "attack");
    add_tv(t, "defends",    "defend",    "defend");
    add_tv(t, "saves",      "save",      "save");
    add_tv(t, "destroys",   "destroy",   "destroy");
    add_tv(t, "creates",    "create",    "create");
    add_tv(t, "discovers",  "discover",  "discover");
    add_tv(t, "invents",    "invent",    "invent");
    add_tv(t, "describes",  "describe",  "describe");
    add_tv(t, "explains",   "explain",   "explain");
    add_tv(t, "considers",  "consider",  "consider");
    add_tv(t, "remembers",  "remember",  "remember");
    add_tv(t, "forgets",    "forget",    "forget");
    add_tv(t, "imagines",   "imagine",   "imagine");
    add_tv(t, "believes",   "believe",   "believe");
    add_tv(t, "doubts",     "doubt",     "doubt");
    add_tv(t, "expects",    "expect",    "expect");
    add_tv(t, "suggests",   "suggest",   "suggest");
    add_tv(t, "requests",   "request",   "request");
    add_tv(t, "demands",    "demand",    "demand");
    add_tv(t, "allows",     "allow",     "allow");
    add_tv(t, "prevents",   "prevent",   "prevent");
    add_tv(t, "causes",     "cause",     "cause");
    add_tv(t, "affects",    "affect",    "affect");
    add_tv(t, "involves",   "involve",   "involve");
    add_tv(t, "concerns",   "concern",   "concern");
    add_tv(t, "interests",  "interest",  "interest");
    add_tv(t, "surprises",  "surprise",  "surprise");
    add_tv(t, "pleases",    "please",    "please");
    add_tv(t, "satisfies",  "satisfy",   "satisfy");
    add_tv(t, "disappoints","disappoint","disappoint");
    add_tv(t, "confuses",   "confuse",   "confuse");
    add_tv(t, "impresses",  "impress",   "impress");
    add_tv(t, "comforts",   "comfort",   "comfort");
    add_tv(t, "warns",      "warn",      "warn");
    add_tv(t, "guides",     "guide",     "guide");
    add_tv(t, "protects",   "protect",   "protect");
    add_tv(t, "threatens",  "threaten",  "threaten");
    add_tv(t, "challenges", "challenge", "challenge");
    add_tv(t, "greets",     "greet",     "greet");
    add_tv(t, "welcomes",   "welcome",   "welcome");
    add_tv(t, "visits",     "visit",     "visit");
    add_tv(t, "hears",      "hear",      "hear");
    add_tv(t, "obeys",      "obey",      "obey");
    add_tv(t, "betrays",    "betray",    "betray");
    add_tv(t, "rewards",    "reward",    "reward");
    add_tv(t, "punishes",   "punish",    "punish");
    add_tv(t, "judges",     "judge",     "judge");
    // study/studies: both IV ("she studies") and TV ("she studies math")
    t["studies"] = {{ cat_SbNP(), t_intrans("study") }, { cat_SbNP_fNP(), t_trans("study") }};
    t["study"]   = {{ cat_SbNP(), t_intrans("study") }, { cat_SbNP_fNP(), t_trans("study") }};
    add_tv(t, "questions",  "question",  "question");
    add_tv(t, "answers",    "answer",    "answer");
    add_tv(t, "solves",     "solve",     "solve");
    add_tv(t, "admires",    "admire",    "admire");
    add_tv(t, "envies",     "envy",      "envy");
    add_tv(t, "understands","understand","understand");
    add_tv(t, "accepts",    "accept",    "accept");
    add_tv(t, "rejects",    "reject",    "reject");
    add_tv(t, "ignores",    "ignore",    "ignore");
    add_tv(t, "avoids",     "avoid",     "avoid");
    add_tv(t, "misses",     "miss",      "miss");
    add_tv(t, "blames",     "blame",     "blame");
    add_tv(t, "praises",    "praise",    "praise");
    add_tv(t, "curses",     "curse",     "curse");
    add_tv(t, "steals",     "steal",     "steal");
    add_tv(t, "reveals",    "reveal",    "reveal");
    add_tv(t, "guards",     "guard",     "guard");
    add_tv(t, "enters",     "enter",     "enter");
    add_tv(t, "leaves",     "leave",     "leave");
    add_tv(t, "crosses",    "cross",     "cross");
    add_tv(t, "reaches",    "reach",     "reach");
    add_tv(t, "fills",      "fill",      "fill");
    add_tv(t, "empties",    "empty",     "empty");
    add_tv(t, "lights",     "light",     "light");
    add_tv(t, "wears",      "wear",      "wear");
    add_tv(t, "draws",      "draw",      "draw");
    add_tv(t, "paints",     "paint",     "paint");
    add_tv(t, "names",      "name",      "name");
    add_tv(t, "counts",     "count",     "count");
    add_tv(t, "measures",   "measure",   "measure");
    add_tv(t, "raises",     "raise",     "raise");
    add_tv(t, "lowers",     "lower",     "lower");
    add_tv(t, "releases",   "release",   "release");
    add_tv(t, "captures",   "capture",   "capture");
    add_tv(t, "rescues",    "rescue",    "rescue");
    add_tv(t, "confronts",  "confront",  "confront");
    add_tv(t, "examines",   "examine",   "examine");
    add_tv(t, "observes",   "observe",   "observe");
    add_tv(t, "recognizes", "recognize", "recognize");
    add_tv(t, "chooses",    "choose",    "choose");
    add_tv(t, "collects",   "collect",   "collect");
    add_tv(t, "gathers",    "gather",    "gather");
    add_tv(t, "shares",     "share",     "share");
    add_tv(t, "offers",     "offer",     "offer");
    add_tv(t, "receives",   "receive",   "receive");
    add_tv(t, "watches",    "watch",     "watch");
    add_tv(t, "learns",     "learn",     "learn");

    // Past tense, transitive (irregular)
    add_tv_past(t, "liked",      "like");
    add_tv_past(t, "saw",        "see");
    add_tv_past(t, "loved",      "love");
    add_tv_past(t, "hated",      "hate");
    add_tv_past(t, "knew",       "know");
    add_tv_past(t, "found",      "find");
    add_tv_past(t, "made",       "make");
    add_tv_past(t, "took",       "take");
    add_tv_past(t, "got",        "get");
    add_tv_past(t, "kept",       "keep");
    add_tv_past(t, "held",       "hold");
    add_tv_past(t, "met",        "meet");
    add_tv_past(t, "beat",       "beat");
    add_tv_past(t, "killed",     "kill");
    add_tv_past(t, "hit",        "hit");
    add_tv_past(t, "wrote",      "write");
    add_tv_past(t, "built",      "build");
    add_tv_past(t, "broke",      "break");
    add_tv_past(t, "bought",     "buy");
    add_tv_past(t, "sold",       "sell");
    add_tv_past(t, "brought",    "bring");
    add_tv_past(t, "led",        "lead");
    add_tv_past(t, "sought",     "seek");
    add_tv_past(t, "heard",      "hear");
    add_tv_past(t, "wore",       "wear");
    add_tv_past(t, "drew",       "draw");
    add_tv_past(t, "chose",      "choose");
    add_tv_past(t, "lost",       "lose");
    add_tv_past(t, "won",        "win");
    add_tv_past(t, "stole",      "steal");
    add_tv_past(t, "taught",     "teach");
    add_tv_past(t, "caught",     "catch");
    add_tv_past(t, "sent",       "send");
    add_tv_past(t, "read",       "read");
    add_tv_past(t, "thought",    "think");  // also IV past
    add_tv_past(t, "said",       "say");
    add_tv_past(t, "told",       "tell");
    add_tv_past(t, "chased",     "chase");
    add_tv_past(t, "helped",     "help");
    add_tv_past(t, "used",       "use");
    add_tv_past(t, "wanted",     "want");
    add_tv_past(t, "needed",     "need");
    add_tv_past(t, "called",     "call");
    add_tv_past(t, "followed",   "follow");

    // -----------------------------------------------------------------------
    // Ditransitive verbs
    // -----------------------------------------------------------------------
    add_dtv(t, "gives",    "give",    "give");
    add_dtv(t, "sends",    "send",    "send");
    add_dtv(t, "tells",    "tell",    "tell");
    add_dtv(t, "shows",    "show",    "show");
    add_dtv(t, "lends",    "lend",    "lend");
    add_dtv(t, "hands",    "hand",    "hand");
    add_dtv(t, "owes",     "owe",     "owe");
    add_dtv(t, "grants",   "grant",   "grant");
    add_dtv(t, "awards",   "award",   "award");
    add_dtv(t, "promises", "promise", "promise");
    add_dtv(t, "denies",   "deny",    "deny");

    // Past tense, ditransitive
    add_dtv_past(t, "gave",     "give");
    add_dtv_past(t, "showed",   "show");
    add_dtv_past(t, "lent",     "lend");
    add_dtv_past(t, "granted",  "grant");
    add_dtv_past(t, "awarded",  "award");
    add_dtv_past(t, "promised", "promise");
    add_dtv_past(t, "denied",   "deny");

    // -----------------------------------------------------------------------
    // Adjectives  (N/N + S\NP)
    // -----------------------------------------------------------------------
    for (auto& w : {
        "big", "small", "large", "little", "tiny", "huge", "vast",
        "tall", "short", "long", "wide", "narrow", "deep", "shallow",
        "high", "low", "thick", "thin",
        "young", "old", "new", "ancient", "modern", "early", "late",
        "good", "bad", "great", "fine", "excellent", "poor", "terrible",
        "dark", "light", "bright", "dim", "pale", "golden", "silver",
        "hot", "cold", "warm", "cool", "frozen",
        "fast", "slow", "quick", "swift",
        "hard", "soft", "solid",
        "strong", "weak", "powerful", "gentle",
        "rich", "wealthy", "humble",
        "happy", "sad", "angry", "calm", "proud", "ashamed",
        "clever", "wise", "foolish", "smart",
        "brave", "cowardly", "bold", "timid",
        "kind", "cruel", "harsh", "fair",
        "free", "bound", "open", "closed",
        "full", "empty", "whole", "broken",
        "true", "false", "real", "certain",
        "clear", "pure", "dirty", "clean",
        "dead", "alive",
        "wild", "tame", "fierce", "peaceful",
        "holy", "sacred", "evil", "wicked",
        "beautiful", "ugly", "lovely",
        "strange", "common", "rare", "ordinary", "special",
        "secret", "hidden", "visible", "invisible",
        "silent", "loud", "quiet",
        "safe", "dangerous",
        "simple", "easy", "difficult",
        "right", "wrong", "correct",
        "famous", "unknown", "noble",
        "lost", "found",
        "heavy", "flat", "round", "straight"
    }) {
        add_adj(t, w);
    }

    // -----------------------------------------------------------------------
    // VP adverbs  (S\NP)\(S\NP)
    // -----------------------------------------------------------------------
    for (auto& w : {
        "quickly", "slowly", "carefully", "silently", "loudly",
        "suddenly", "gently", "fiercely", "bravely", "wisely",
        "foolishly", "kindly", "cruelly", "proudly", "sadly",
        "happily", "angrily", "calmly", "freely", "deeply",
        "softly", "strongly", "weakly", "swiftly", "boldly",
        "secretly", "openly", "clearly", "blindly", "wildly",
        "soundly", "brightly", "darkly", "loudly", "tightly",
        "closely", "roughly", "simply", "surely", "nearly",
        "always", "often", "rarely", "sometimes", "usually", "seldom"
    }) {
        t[w] = {{ VPbV, t_vpadv(w) }};
    }

    // -----------------------------------------------------------------------
    // Prepositions, VP adjunct + N post-modifier
    // -----------------------------------------------------------------------
    for (auto& [w, pred] : std::initializer_list<std::pair<const char*,const char*>>{
        {"in",         "in"},   {"on",        "on"},   {"at",        "at"},
        {"by",         "by"},   {"with",      "with"}, {"from",      "from"},
        {"to",         "to"},   {"through",   "through"},
        {"near",       "near"}, {"under",     "under"}, {"over",     "over"},
        {"above",      "above"},{"below",     "below"}, {"behind",   "behind"},
        {"before",     "before"},{"after",    "after"}, {"between",  "between"},
        {"among",      "among"},{"around",    "around"},{"without",  "without"},
        {"within",     "within"},{"against",  "against"},{"about",   "about"},
        {"along",      "along"},{"across",    "across"},{"beside",   "beside"},
        {"beyond",     "beyond"},{"inside",   "inside"},{"outside",  "outside"},
        {"toward",     "toward"},{"towards",  "toward"},{"upon",     "upon"},
        {"onto",       "onto"}, {"into",      "into"},  {"during",   "during"},
        {"beneath",    "beneath"},{"despite", "despite"},{"past",    "past"},
        {"since",      "since"},{"until",     "until"}, {"for",      "for"},
        {"of",         "of"},   {"off",       "off"},   {"except",   "except"}
    }) {
        add_prep(t, w, pred);
    }

    // -----------------------------------------------------------------------
    // Proper names (GQ-lifted)
    // -----------------------------------------------------------------------
    for (auto& n : {
        // Male
        "john", "bob", "tom", "jim", "jack", "james", "william", "henry",
        "george", "charles", "david", "michael", "robert", "richard",
        "edward", "thomas", "peter", "paul", "mark", "luke", "matthew",
        "andrew", "philip", "simon", "joseph", "benjamin", "samuel",
        "daniel", "alexander", "oliver", "harry", "arthur", "alfred",
        "albert", "frederick", "edmund", "leonard", "harold", "lawrence",
        "vincent", "walter", "stanley", "ernest", "frank", "claude",
        "ralph", "oscar", "victor", "felix", "adam", "alan",
        "carl", "eric", "evan", "hugo", "ian", "ivan", "karl",
        "leo", "leon", "liam", "lucas", "max", "neil", "noel",
        "omar", "owen", "ryan", "sean", "seth", "theo", "tyler", "wade",
        // Female
        "mary", "alice", "jane", "ann", "kate", "emma", "lucy", "sarah",
        "emily", "elizabeth", "helen", "margaret", "dorothy", "eleanor",
        "florence", "grace", "ruth", "rose", "violet", "claire", "diana",
        "eve", "faith", "gloria", "harriet", "isabel", "julia", "karen",
        "laura", "mabel", "nancy", "olivia", "patricia", "rachel", "sophie",
        "teresa", "ursula", "victoria", "wendy", "ada", "agatha", "agnes",
        "anna", "beatrice", "cecilia", "charlotte", "clara", "constance",
        "edith", "elena", "esther", "eva", "frances", "iris", "joan",
        "joyce", "june", "katharine", "lily", "lydia", "mae", "maud",
        "miriam", "nora", "norma", "paula", "stella", "sylvia", "vera"
    }) {
        t[n] = {{ CAT_NP, t_proper(n) }};
    }

    return t;
}

static const Table LEXICON = build_table();

// ---------------------------------------------------------------------------
// Simple rule-based morphological fallback
// Handles common English inflection not listed explicitly.
// ---------------------------------------------------------------------------
static std::vector<LexEntry> morph_fallback(const std::string& key) {
    auto try_stem = [&](const std::string& stem) -> std::vector<LexEntry> {
        auto it = LEXICON.find(stem);
        return it != LEXICON.end() ? it->second : std::vector<LexEntry>{};
    };

    // -ed past tense / past participle
    if (key.size() > 3 && key.substr(key.size()-2) == "ed") {
        // loved -> love, chased -> chase
        auto s1 = try_stem(key.substr(0, key.size()-1));
        if (!s1.empty()) return s1;
        // walked -> walk, talked -> talk
        auto s2 = try_stem(key.substr(0, key.size()-2));
        if (!s2.empty()) return s2;
        // tried -> try (ied -> y)
        if (key.size() > 4 && key.substr(key.size()-3) == "ied") {
            auto s3 = try_stem(key.substr(0, key.size()-3) + "y");
            if (!s3.empty()) return s3;
        }
    }
    // -ing
    if (key.size() > 4 && key.substr(key.size()-3) == "ing") {
        // running -> run (consonant doubling already stripped by removing 1 char)
        auto s1 = try_stem(key.substr(0, key.size()-3));
        if (!s1.empty()) return s1;
        // liking -> like (silent e dropped)
        auto s2 = try_stem(key.substr(0, key.size()-3) + "e");
        if (!s2.empty()) return s2;
    }
    // -s/-es plurals / 3sg
    if (key.size() > 3 && key.substr(key.size()-2) == "es") {
        // boxes -> box, chases -> chase
        auto s1 = try_stem(key.substr(0, key.size()-2));
        if (!s1.empty()) return s1;
        auto s2 = try_stem(key.substr(0, key.size()-1));
        if (!s2.empty()) return s2;
    }
    if (key.size() > 2 && key.back() == 's') {
        auto s1 = try_stem(key.substr(0, key.size()-1));
        if (!s1.empty()) return s1;
    }
    // -ies -> -y (flies -> fly)
    if (key.size() > 3 && key.substr(key.size()-3) == "ies") {
        auto s1 = try_stem(key.substr(0, key.size()-3) + "y");
        if (!s1.empty()) return s1;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
std::vector<LexEntry> lexicon_lookup(const std::string& word) {
    std::string key = word;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    auto it = LEXICON.find(key);
    if (it != LEXICON.end()) return it->second;

    // Try morphological fallback
    auto morph = morph_fallback(key);
    if (!morph.empty()) return morph;

    // Unknown word: treat as noun
    return {{ CAT_N, t_noun(key) }};
}
