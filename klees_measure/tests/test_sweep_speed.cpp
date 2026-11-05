#include <iostream>
#include <chrono>
#include <vector>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

int main() {
    std::cout << "Testing exact_kmp_sweep speed..." << std::endl;
    for (int d = 4; d <= 6; ++d) {
        for (int k : {10, 15, 20, 25, 30, 40, 50}) {
            auto boxes = generate_trellis_weave(k, d, 42);
            auto t0 = std::chrono::high_resolution_clock::now();
            Int128 v = exact_kmp_sweep(boxes, d);
            auto t1 = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            std::cout << "d=" << d << " k=" << k << " sweep=" << us << " us" << std::endl;
            if (us > 50000.0) break; // > 50ms
        }
    }
    return 0;
}
