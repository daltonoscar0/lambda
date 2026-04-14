#include "src/parser.hpp"
#include "src/pretty_print.hpp"
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

static std::string tree_to_json(const DerivTree& node, int indent = 0) {
    if (!node) return "null";
    std::string pad(indent * 2, ' ');
    std::string inner(indent * 2 + 2, ' ');
    std::string out = "{\n";
    out += inner + "\"rule\": \"" + json_escape(node->rule) + "\",\n";
    out += inner + "\"cat\": \"" + json_escape(cat_repr(node->cat)) + "\"";
    if (!node->word.empty())
        out += ",\n" + inner + "\"word\": \"" + json_escape(node->word) + "\"";
    if (!node->children.empty()) {
        out += ",\n" + inner + "\"children\": [\n";
        for (size_t i = 0; i < node->children.size(); ++i) {
            out += inner + "  " + tree_to_json(node->children[i], indent + 2);
            if (i + 1 < node->children.size()) out += ",";
            out += "\n";
        }
        out += inner + "]";
    }
    out += "\n" + pad + "}";
    return out;
}

// ---------------------------------------------------------------------------
// Derivation tree printer (ASCII box-drawing)
// ---------------------------------------------------------------------------
static void print_tree(const DerivTree& node, const std::string& prefix = "",
                       bool last = true) {
    if (!node) return;
    std::string connector = last ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ";
    std::string child_prefix = prefix + (last ? "    " : "\u2502   ");
    std::string label = "[" + node->rule + "] " + cat_repr(node->cat);
    if (!node->word.empty()) label += " \"" + node->word + "\"";
    std::cout << prefix << connector << label << "\n";
    for (size_t i = 0; i < node->children.size(); ++i)
        print_tree(node->children[i], child_prefix, i + 1 == node->children.size());
}

// ---------------------------------------------------------------------------
// Process one sentence
// ---------------------------------------------------------------------------
static void process(const std::string& sentence, bool show_steps,
                    bool show_tree, bool json_out, bool all_parses) {
    std::string s = sentence;
    size_t a = s.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return;
    size_t b = s.find_last_not_of(" \t\n\r");
    s = s.substr(a, b - a + 1);
    if (s.empty()) return;

    // ---- JSON output -------------------------------------------------------
    if (json_out) {
        std::cout << "{\n  \"input\": \"" << json_escape(s) << "\",\n  \"parses\": [\n";
        try {
            auto results = parse(s);
            for (size_t i = 0; i < results.size(); ++i) {
                bool is_q = cat_equal(results[i].cat, CAT_Q);
                std::cout << "    {\n"
                          << "      \"type\": \"" << (is_q ? "Question" : "Statement") << "\",\n"
                          << "      \"logical_form\": \"" << json_escape(pretty(results[i].sem)) << "\"";
                if (results[i].deriv) {
                    std::cout << ",\n      \"tree\": " << tree_to_json(results[i].deriv, 3);
                }
                std::cout << "\n    }";
                if (i + 1 < results.size()) std::cout << ",";
                std::cout << "\n";
            }
        } catch (const ParseError& e) {
            std::cout << "    {\"error\": \"" << json_escape(e.what()) << "\"}\n";
        }
        std::cout << "  ]\n}\n";
        return;
    }

    // ---- Normal / step output ----------------------------------------------
    std::cout << "\nInput: " << s << "\n";
    std::cout << std::string(s.size() + 8, '-') << "\n";

    if (show_steps) {
        try {
            auto results = parse_steps(s);
            for (size_t i = 0; i < results.size(); ++i) {
                bool multi = all_parses || results.size() > 1;
                std::string label = multi ? "Parse " + std::to_string(i+1)
                                          : "Logical Form";
                std::cout << "\n" << label << " \u2014 reduction steps:\n";
                for (size_t j = 0; j < results[i].steps.size(); ++j) {
                    std::cout << "  Step " << std::setw(2) << j
                              << ": " << pretty(results[i].steps[j]) << "\n";
                }
                std::cout << "  Final:  " << pretty(results[i].steps.back()) << "\n";
            }
        } catch (const ParseError& e) {
            std::cerr << "Parse failed: " << e.what() << "\n";
        }
        return;
    }

    try {
        auto results = parse(s);
        for (size_t i = 0; i < results.size(); ++i) {
            bool multi = all_parses || results.size() > 1;
            bool is_q  = cat_equal(results[i].cat, CAT_Q);
            std::string label;
            if (multi) {
                label = "Parse " + std::to_string(i+1)
                      + (is_q ? " (Question)" : "");
            } else {
                label = is_q ? "Question" : "Logical Form";
            }
            std::cout << "\n" << label << ": " << pretty(results[i].sem) << "\n";

            if (show_tree && results[i].deriv) {
                std::cout << "\nDerivation:\n";
                print_tree(results[i].deriv);
            }
        }
    } catch (const ParseError& e) {
        std::cerr << "Parse failed: " << e.what() << "\n";
    }
}

// ---------------------------------------------------------------------------
// Interactive REPL
// ---------------------------------------------------------------------------
static void interactive(bool show_steps, bool show_tree, bool json_out, bool all_parses) {
    std::cout << "lambda. \u2014 interactive mode\n";
    std::cout << "Type a sentence and press Enter. Type 'quit' to exit.\n\n";
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "quit" || line == "exit" || line == "q") break;
        process(line, show_steps, show_tree, json_out, all_parses);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    bool show_steps  = false;
    bool show_tree   = false;
    bool json_out    = false;
    bool all_parses  = false;
    bool do_interact = false;
    std::string sentence;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--show-steps")               show_steps  = true;
        else if (arg == "--show-tree")                show_tree   = true;
        else if (arg == "--json")                     json_out    = true;
        else if (arg == "--all-parses")               all_parses  = true;
        else if (arg == "--interactive" || arg == "-i") do_interact = true;
        else if (arg[0] != '-')                       sentence    = arg;
    }

    if (do_interact || (sentence.empty() && isatty(0))) {
        interactive(show_steps, show_tree, json_out, all_parses);
        return 0;
    }

    if (!sentence.empty()) {
        process(sentence, show_steps, show_tree, json_out, all_parses);
        return 0;
    }

    // Batch: read from stdin
    std::string line;
    while (std::getline(std::cin, line))
        process(line, show_steps, show_tree, json_out, all_parses);

    return 0;
}
