#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include "../harness/types.hpp"
#include "../harness/generators.hpp"
#include "../harness/verifier.hpp"
#include "kmp_solver.hpp"
#include "split_policy.hpp"

using namespace kmp;

// Compute empirical scaling exponent alpha_hat = OLS slope of log(y) vs log(x)
inline double compute_alpha_hat(const std::vector<double>& x_vals, const std::vector<double>& y_vals) {
    if (x_vals.size() < 2 || x_vals.size() != y_vals.size()) return 0.0;
    double sum_lx = 0.0, sum_ly = 0.0, sum_lx_ly = 0.0, sum_lx2 = 0.0;
    size_t m = x_vals.size();
    for (size_t i = 0; i < m; ++i) {
        double lx = std::log(x_vals[i]);
        double ly = std::log(std::max(1.0, y_vals[i]));
        sum_lx += lx;
        sum_ly += ly;
        sum_lx_ly += lx * ly;
        sum_lx2 += lx * lx;
    }
    double denom = (m * sum_lx2 - sum_lx * sum_lx);
    if (std::abs(denom) < 1e-12) return 0.0;
    return (m * sum_lx_ly - sum_lx * sum_ly) / denom;
}

int main(int argc, char** argv) {
    std::string mode = "bench";
    int d = 4;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "-d" && i + 1 < argc) {
            d = std::stoi(argv[++i]);
        }
    }

    SplitPolicy& policy = get_split_policy();

    if (mode == "test") {
        // Run exact ground-truth verification on multiple seeds
        for (int test_d : {2, 3, 4}) {
            for (uint64_t seed = 100; seed < 108; ++seed) {
                int n_boxes = (test_d == 4 ? 20 : 35);
                auto boxes = generate_small_test(n_boxes, test_d, seed);
                Int128 ref_vol = exact_verify_volume(boxes);
                StructuralCounters counters;
                Int128 sol_vol = KMPSolver::solve(boxes, counters, policy);
                if (ref_vol != sol_vol) {
                    std::cout << "{\n"
                              << "  \"status\": \"VERIFICATION_FAILED\",\n"
                              << "  \"d\": " << test_d << ",\n"
                              << "  \"seed\": " << seed << ",\n"
                              << "  \"ref_volume\": \"" << int128_to_string(ref_vol) << "\",\n"
                              << "  \"solver_volume\": \"" << int128_to_string(sol_vol) << "\"\n"
                              << "}" << std::endl;
                    return 1;
                }
            }
        }
        std::cout << "{\n  \"status\": \"VERIFICATION_SUCCESS\"\n}" << std::endl;
        return 0;
    }

    if (mode == "bench") {
        std::vector<int> n_list = {64, 128, 256, 512};
        std::vector<double> n_doubles = {64.0, 128.0, 256.0, 512.0};

        std::vector<double> trellis_faces;
        std::vector<double> clique_faces;

        uint64_t total_faces = 0;
        uint64_t total_calls = 0;
        uint64_t total_leaves = 0;

        // 1. Orthogonal Trellis Weaves
        for (int n : n_list) {
            auto boxes = generate_trellis_weave(n, d, 42);
            StructuralCounters counters;
            KMPSolver::solve(boxes, counters, policy);
            trellis_faces.push_back((double)counters.total_d2_faces_processed);
            total_faces += counters.total_d2_faces_processed;
            total_calls += counters.recursive_calls;
            total_leaves += counters.trellis_leaves;
        }

        // 2. Hyperclique Complement Gadgets
        for (int n : n_list) {
            auto boxes = generate_hyperclique_gadget(n, d, 999);
            StructuralCounters counters;
            KMPSolver::solve(boxes, counters, policy);
            clique_faces.push_back((double)counters.total_d2_faces_processed);
            total_faces += counters.total_d2_faces_processed;
            total_calls += counters.recursive_calls;
            total_leaves += counters.trellis_leaves;
        }

        double alpha_trellis = compute_alpha_hat(n_doubles, trellis_faces);
        double alpha_clique = compute_alpha_hat(n_doubles, clique_faces);

        std::vector<double> combined_faces(n_list.size());
        for (size_t i = 0; i < n_list.size(); ++i) {
            combined_faces[i] = trellis_faces[i] + clique_faces[i];
        }
        double alpha_hat = compute_alpha_hat(n_doubles, combined_faces);

        std::cout << std::fixed << std::setprecision(5);
        std::cout << "{\n"
                  << "  \"policy_id\": \"" << policy.id << "\",\n"
                  << "  \"d\": " << d << ",\n"
                  << "  \"total_d2_faces_processed\": " << total_faces << ",\n"
                  << "  \"alpha_hat\": " << alpha_hat << ",\n"
                  << "  \"alpha_trellis\": " << alpha_trellis << ",\n"
                  << "  \"alpha_clique\": " << alpha_clique << ",\n"
                  << "  \"recursive_calls\": " << total_calls << ",\n"
                  << "  \"trellis_leaves\": " << total_leaves << ",\n"
                  << "  \"trellis_d2_512\": " << (uint64_t)trellis_faces.back() << ",\n"
                  << "  \"clique_d2_512\": " << (uint64_t)clique_faces.back() << "\n"
                  << "}" << std::endl;
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << std::endl;
    return 2;
}
