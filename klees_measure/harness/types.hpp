#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace kmp {

// Portable exact 128-bit unsigned/signed integer for 32-bit & 64-bit targets
struct Int128 {
    uint64_t lo;
    int64_t hi;

    Int128() : lo(0), hi(0) {}
    Int128(int64_t val) : lo((uint64_t)val), hi(val < 0 ? -1 : 0) {}
    Int128(uint64_t l, int64_t h) : lo(l), hi(h) {}

    bool operator==(const Int128& o) const { return lo == o.lo && hi == o.hi; }
    bool operator!=(const Int128& o) const { return !(*this == o); }
    bool operator<(const Int128& o) const {
        if (hi != o.hi) return hi < o.hi;
        return lo < o.lo;
    }
    bool operator<=(const Int128& o) const { return *this < o || *this == o; }
    bool operator>(const Int128& o) const { return !(*this <= o); }
    bool operator>=(const Int128& o) const { return !(*this < o); }

    Int128 operator+(const Int128& o) const {
        uint64_t res_lo = lo + o.lo;
        int64_t carry = (res_lo < lo) ? 1 : 0;
        int64_t res_hi = hi + o.hi + carry;
        return Int128(res_lo, res_hi);
    }

    Int128& operator+=(const Int128& o) {
        *this = *this + o;
        return *this;
    }

    Int128 operator-(const Int128& o) const {
        uint64_t res_lo = lo - o.lo;
        int64_t borrow = (lo < o.lo) ? 1 : 0;
        int64_t res_hi = hi - o.hi - borrow;
        return Int128(res_lo, res_hi);
    }

    // Multiply non-negative Int128 by non-negative int64_t
    Int128 multiply_int64(int64_t m) const {
        if (m == 0 || (lo == 0 && hi == 0)) return Int128(0);
        uint64_t m_u = (uint64_t)m;

        uint64_t a0 = lo & 0xFFFFFFFFULL;
        uint64_t a1 = lo >> 32;
        uint64_t b0 = m_u & 0xFFFFFFFFULL;
        uint64_t b1 = m_u >> 32;

        uint64_t p00 = a0 * b0;
        uint64_t p01 = a0 * b1;
        uint64_t p10 = a1 * b0;
        uint64_t p11 = a1 * b1;

        uint64_t mid = p01 + p10;
        uint64_t carry_mid = (mid < p01) ? 1ULL : 0ULL;

        uint64_t low_mid = mid << 32;
        uint64_t high_mid = (mid >> 32) | (carry_mid << 32);

        uint64_t res_lo = p00 + low_mid;
        uint64_t carry_lo = (res_lo < p00) ? 1ULL : 0ULL;

        int64_t res_hi = (int64_t)(p11 + high_mid + carry_lo + (uint64_t)hi * m_u);
        return Int128(res_lo, res_hi);
    }

    Int128 operator*(int64_t m) const {
        return multiply_int64(m);
    }

    std::string to_string() const {
        if (hi == 0 && lo == 0) return "0";
        if (hi == 0) return std::to_string(lo);
        // Repeated division by 10 for exact string representation
        Int128 temp = *this;
        std::string s;
        while (temp.hi > 0 || temp.lo > 0) {
            // Divide temp by 10
            uint64_t rem = 0;
            // High part
            uint64_t h = (uint64_t)temp.hi;
            rem = h % 10;
            uint64_t new_hi = h / 10;
            // Low part
            uint64_t l0 = (temp.lo >> 32);
            uint64_t dividend0 = (rem << 32) | l0;
            uint64_t q0 = dividend0 / 10;
            rem = dividend0 % 10;

            uint64_t l1 = temp.lo & 0xFFFFFFFFULL;
            uint64_t dividend1 = (rem << 32) | l1;
            uint64_t q1 = dividend1 / 10;
            rem = dividend1 % 10;

            s.push_back('0' + (int)rem);
            temp.hi = (int64_t)new_hi;
            temp.lo = (q0 << 32) | q1;
        }
        std::reverse(s.begin(), s.end());
        return s;
    }
};

inline std::string int128_to_string(const Int128& val) {
    return val.to_string();
}

struct Box {
    std::vector<int64_t> low;
    std::vector<int64_t> high;

    int dim() const { return (int)low.size(); }

    bool is_valid() const {
        for (size_t i = 0; i < low.size(); ++i) {
            if (high[i] <= low[i]) return false;
        }
        return true;
    }

    Int128 volume() const {
        if (!is_valid()) return Int128(0);
        Int128 vol(1);
        for (size_t i = 0; i < low.size(); ++i) {
            vol = vol * (high[i] - low[i]);
        }
        return vol;
    }

    Box intersect(const Box& other) const {
        int d = dim();
        Box res;
        res.low.resize(d);
        res.high.resize(d);
        for (int i = 0; i < d; ++i) {
            res.low[i] = std::max(low[i], other.low[i]);
            res.high[i] = std::min(high[i], other.high[i]);
        }
        return res;
    }

    bool contains(const Box& inner) const {
        for (size_t i = 0; i < low.size(); ++i) {
            if (low[i] > inner.low[i] || high[i] < inner.high[i]) return false;
        }
        return true;
    }
};

struct Corner2Cut {
    int axis1 = -1;
    int axis2 = -1;
    int64_t cut1 = 0;
    int64_t cut2 = 0;
    bool is_valid = false;
};

struct StructuralCounters {
    Int128 exact_volume = Int128(0);
    uint64_t total_d2_faces_processed = 0;
    uint64_t recursive_calls = 0;
    uint64_t trellis_leaves = 0;
    uint64_t simplify_steps = 0;

    void reset() {
        exact_volume = Int128(0);
        total_d2_faces_processed = 0;
        recursive_calls = 0;
        trellis_leaves = 0;
        simplify_steps = 0;
    }
};

} // namespace kmp
