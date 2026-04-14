#pragma once
#include "test_runner.hpp"
#include "../src/parser.hpp"
#include <fstream>
#include <string>

inline void test_benchmark(TestRunner& R) {
    // Locate benchmark file relative to the binary's working directory
    const std::string path = "tests/benchmark.txt";
    std::ifstream f(path);
    if (!f.is_open()) {
        // Try one level up (when running from build/)
        std::ifstream f2("../tests/benchmark.txt");
        if (!f2.is_open()) {
            R.suite("benchmark — file load");
            CHECK(R, false);  // file not found
            return;
        }
    }

    // Re-open knowing file exists (handle both locations)
    std::ifstream file(path);
    if (!file.is_open()) file.open("../tests/benchmark.txt");

    std::string line;
    int sentence_count = 0;
    while (std::getline(file, line)) {
        // Skip blank lines and comments
        if (line.empty() || line[0] == '#') continue;
        // Trim trailing whitespace
        size_t end = line.find_last_not_of(" \t\r");
        if (end != std::string::npos) line = line.substr(0, end + 1);
        if (line.empty()) continue;

        ++sentence_count;
        R.suite("benchmark — " + line);
        try {
            auto results = parse(line);
            CHECK(R, !results.empty());
        } catch (const ParseError&) {
            CHECK(R, false);
        }
    }

    // Sanity check: we should have at least 50 sentences
    R.suite("benchmark — sentence count >= 50");
    CHECK(R, sentence_count >= 50);
}
