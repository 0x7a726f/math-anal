#include <iostream>
#include <chrono>
#include <vector>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"

using namespace kmp;

int main() {
    auto boxes = generate_trellis_weave(512, 5, 42);
    std::cout << "Testing heap allocations in loop..." << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < 50000; ++it) {
        std::vector<std::vector<uint32_t>> m(5, std::vector<uint32_t>(5, 0));
        m[0][1] = 1;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_heap = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto t2 = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < 50000; ++it) {
        uint32_t m[6][6] = {0};
        m[0][1] = 1;
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    double ms_stack = std::chrono::duration<double, std::milli>(t3 - t2).count();

    std::cout << "50,000 vector<vector> heap allocations: " << ms_heap << " ms" << std::endl;
    std::cout << "50,000 stack array allocations:         " << ms_stack << " ms" << std::endl;
    std::cout << "Speedup: " << (ms_heap / ms_stack) << "x" << std::endl;
    return 0;
}
