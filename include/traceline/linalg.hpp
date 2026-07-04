#pragma once

// Minimal fixed-size matrix operations for the fusion engine.
//
// Deliberately not using Eigen or any external linear-algebra library:
// for a system whose whole premise is bounded-time, no-heap-allocation
// behavior (REQ-001, REQ-002), pulling in a general-purpose matrix
// library is the wrong tradeoff -- it brings allocation paths and
// runtime-sized operations we'd then have to audit away. Hand-rolled,
// fixed-size, stack-only matrices make the "no allocation, bounded
// time" property structurally obvious rather than something we have
// to trust a third-party library to preserve.

#include "traceline/types.hpp"
#include <array>
#include <cmath>
#include <cstddef>

namespace traceline {

template <std::size_t Rows, std::size_t Cols>
struct Mat {
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    // Deliberately a public-data aggregate, not encapsulated: Mat is a
    // plain-old-data math type (like a POD struct), not an object with
    // invariants to protect. Encapsulating it behind accessors would
    // add no safety here and would get in the way of aggregate
    // initialization used throughout fusion.cpp.
    std::array<double, Rows * Cols> data{};
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
    // Indices here are always in-range by construction: r < Rows and
    // c < Cols are enforced structurally by every call site in this
    // codebase (loop bounds are template parameters, never external
    // input). Bounds-checked access (.at()) would add a throw path,
    // which conflicts with the noexcept, no-exceptions-on-hot-path
    // contract (REQ-006) that this whole module exists to uphold.
    constexpr double& operator()(std::size_t r, std::size_t c) noexcept {
        return data[(r * Cols) + c];
    }
    constexpr double operator()(std::size_t r, std::size_t c) const noexcept {
        return data[(r * Cols) + c];
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
};

template <std::size_t N>
constexpr Mat<N, N> mat_identity() noexcept {
    Mat<N, N> out{};
    for (std::size_t i = 0; i < N; ++i) {
        out(i, i) = 1.0;
    }
    return out;
}

template <std::size_t R, std::size_t K, std::size_t C>
constexpr Mat<R, C> mat_mul(const Mat<R, K>& a, const Mat<K, C>& b) noexcept {
    Mat<R, C> out{};
    for (std::size_t i = 0; i < R; ++i) {
        for (std::size_t j = 0; j < C; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0; k < K; ++k) {
                sum += a(i, k) * b(k, j);
            }
            out(i, j) = sum;
        }
    }
    return out;
}

template <std::size_t R, std::size_t C>
constexpr Mat<C, R> mat_transpose(const Mat<R, C>& a) noexcept {
    Mat<C, R> out{};
    for (std::size_t i = 0; i < R; ++i) {
        for (std::size_t j = 0; j < C; ++j) {
            out(j, i) = a(i, j);
        }
    }
    return out;
}

template <std::size_t R, std::size_t C>
constexpr Mat<R, C> mat_add(const Mat<R, C>& a, const Mat<R, C>& b) noexcept {
    Mat<R, C> out{};
    for (std::size_t i = 0; i < R * C; ++i) {
        out.data[i] = a.data[i] + b.data[i];
    }
    return out;
}

template <std::size_t R, std::size_t C>
constexpr Mat<R, C> mat_sub(const Mat<R, C>& a, const Mat<R, C>& b) noexcept {
    Mat<R, C> out{};
    for (std::size_t i = 0; i < R * C; ++i) {
        out.data[i] = a.data[i] - b.data[i];
    }
    return out;
}

// Analytical 3x3 inverse via the adjugate/cofactor method. Deliberately
// analytical rather than a general Gauss-Jordan routine: fixed
// instruction count, no data-dependent loop bound, which is what
// "bounded time" (REQ-001) needs to mean in practice, not just in
// the benchmark average case.
inline Result<Mat<3, 3>> mat3_inverse(const Mat<3, 3>& m) noexcept {
    const double a = m(0, 0), b = m(0, 1), c = m(0, 2);
    const double d = m(1, 0), e = m(1, 1), f = m(1, 2);
    const double g = m(2, 0), h = m(2, 1), i = m(2, 2);

    const double det = (a * ((e * i) - (f * h))) - (b * ((d * i) - (f * g))) +
                        (c * ((d * h) - (e * g)));

    if (std::abs(det) < 1e-12) {
        return std::unexpected(Error::SingularMatrix);
    }
    const double inv_det = 1.0 / det;

    Mat<3, 3> out{};
    out(0, 0) = ((e * i) - (f * h)) * inv_det;
    out(0, 1) = ((c * h) - (b * i)) * inv_det;
    out(0, 2) = ((b * f) - (c * e)) * inv_det;
    out(1, 0) = ((f * g) - (d * i)) * inv_det;
    out(1, 1) = ((a * i) - (c * g)) * inv_det;
    out(1, 2) = ((c * d) - (a * f)) * inv_det;
    out(2, 0) = ((d * h) - (e * g)) * inv_det;
    out(2, 1) = ((b * g) - (a * h)) * inv_det;
    out(2, 2) = ((a * e) - (b * d)) * inv_det;
    return out;
}

}  // namespace traceline
