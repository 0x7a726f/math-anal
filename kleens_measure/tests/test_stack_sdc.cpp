#include <iostream>
#include <chrono>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"

using namespace kmp;

struct StackSDCBox {
    int64_t low[6];
    int64_t high[6];

    bool is_valid(int d) const {
        for (int i = 0; i < d; ++i) {
            if (high[i] <= low[i]) return false;
        }
        return true;
    }

    Int128 volume(int d) const {
        Int128 v(1);
        for (int i = 0; i < d; ++i) {
            if (high[i] <= low[i]) return Int128(0);
            v = v * (high[i] - low[i]);
        }
        return v;
    }
};

int main() {
    std::cout << "Testing StackSDCBox memory size: " << sizeof(StackSDCBox) << " bytes" << std::endl;
    return 0;
}
