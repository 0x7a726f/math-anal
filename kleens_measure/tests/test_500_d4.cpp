#include <iostream>
#include <chrono>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

int main() {
    auto boxes = generate_trellis_weave(500, 4, 42);
    auto t0 = std::chrono::high_resolution_clock::now();
    Int128 vol = exact_kmp_sweep(boxes, 4);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "500 boxes in d=4 stack sweep: " << ms << " ms" << std::endl;
    return 0;
}
