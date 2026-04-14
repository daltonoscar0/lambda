#pragma once
#include <iostream>
#include <string>
#include <functional>

struct TestRunner {
    int passes = 0, failures = 0;
    std::string current_suite;

    void suite(const std::string& name) { current_suite = name; }

    void check(bool cond, const std::string& expr,
               const char* file, int line) {
        if (cond) {
            ++passes;
        } else {
            ++failures;
            std::cerr << "FAIL [" << file << ":" << line << "] "
                      << current_suite << " — " << expr << "\n";
        }
    }

    int summary() const {
        std::cout << "\n" << passes << " passed, " << failures << " failed.\n";
        return failures > 0 ? 1 : 0;
    }
};

#define CHECK(runner, expr) \
    (runner).check(!!(expr), #expr, __FILE__, __LINE__)

#define CHECK_EQ(runner, a, b) \
    (runner).check((a) == (b), #a " == " #b, __FILE__, __LINE__)
