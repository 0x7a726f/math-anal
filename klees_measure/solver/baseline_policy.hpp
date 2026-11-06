#pragma once
#include "../harness/types.hpp"
#include "../harness/verifier.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace kmp {

struct BaselinePolicy {
    std::string id = "H-00: Baseline Cyclic Chan SDC";

    int thresh_d3 = 64;
    int thresh_d4 = 48;
    int thresh_d5 = 24;
    int thresh_d6 = 18;

    int select_split_axis(
        const Box& cell,
        const std::vector<Box>& boxes,
        const std::vector<std::vector<uint32_t>>& d2_face_matrix,
        int last_split_dim
    ) const {
        return (last_split_dim + 1) % cell.dim();
    }

    int64_t select_cut_coordinate(
        int axis,
        const Box& cell,
        const std::vector<Box>& boxes
    ) const {
        int64_t c_low = cell.low[axis];
        int64_t c_high = cell.high[axis];
        std::vector<int64_t> coords;
        coords.reserve(boxes.size() * 2);

        for (const auto& b : boxes) {
            if (b.low[axis] > c_low && b.low[axis] < c_high) coords.push_back(b.low[axis]);
            if (b.high[axis] > c_low && b.high[axis] < c_high) coords.push_back(b.high[axis]);
        }

        if (coords.empty()) {
            return c_low + (c_high - c_low) / 2;
        }

        std::sort(coords.begin(), coords.end());
        return coords[coords.size() / 2];
    }

    bool should_use_corner_2cut(const Box&, const std::vector<Box>&, uint64_t) const { return false; }
    Corner2Cut select_corner_2cut(const Box&, const std::vector<Box>&, const std::vector<std::vector<uint32_t>>&) const { return {-1, -1, 0, 0, false}; }

    bool should_terminate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        uint64_t active_d2_faces
    ) const {
        if (boxes.empty()) return true;
        int d = cell.dim();
        int threshold = thresh_d3;
        if (d >= 6) threshold = thresh_d6;
        else if (d == 5) threshold = thresh_d5;
        else if (d == 4) threshold = thresh_d4;
        if ((int)boxes.size() <= threshold) return true;
        if (active_d2_faces == 0) return true;
        return false;
    }

    Int128 evaluate_leaf(
        const Box& cell,
        const std::vector<Box>& boxes,
        int d,
        StructuralCounters* counters = nullptr
    ) const {
        if (boxes.empty()) return Int128(0);
        if (boxes.size() == 1) return boxes[0].volume();
        return exact_kmp_sweep(boxes, d, counters);
    }
};

} // namespace kmp
