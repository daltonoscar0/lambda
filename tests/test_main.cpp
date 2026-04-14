#include "test_runner.hpp"
#include "test_lambda.hpp"
#include "test_lexicon.hpp"
#include "test_parser.hpp"
#include "test_benchmark.hpp"

int main() {
    TestRunner R;
    test_lambda(R);
    test_lexicon(R);
    test_parser(R);
    test_benchmark(R);
    return R.summary();
}
