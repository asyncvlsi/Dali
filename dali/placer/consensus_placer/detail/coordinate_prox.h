#pragma once
// Original in-repository numerical kernel; see ../README.md for provenance.
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace dali::consensus_internal {
// Exact weighted bounded isotonic projection after subtracting width prefixes.
// Each row contains one coordinate for every physical owner it covers.
inline std::vector<double>
row_coordinate_prox(const std::vector<double> &t, const std::vector<double> &w,
                    const std::vector<double> &lo,
                    const std::vector<double> &hi,
                    const std::vector<double> &width) {
  size_t n = t.size();
  if (w.size() != n || lo.size() != n || hi.size() != n || width.size() != n)
    throw std::invalid_argument("row dimensions");
  struct Block {
    size_t begin, end;
    double sw, swt, lo, hi, value;
  };
  std::vector<Block> blocks;
  std::vector<double> prefix(n);
  double sum = 0, origin = n ? t[0] : 0;
  for (size_t i = 0; i < n; ++i) {
    if (w[i] <= 0 || lo[i] > hi[i] || width[i] < 0)
      throw std::invalid_argument("row data");
    prefix[i] = sum;
    sum += width[i];
    double l = lo[i] - prefix[i] - origin, h = hi[i] - prefix[i] - origin;
    Block b{i, i + 1, w[i], w[i] * (t[i] - prefix[i] - origin), l, h, 0};
    b.value = std::max(l, std::min(h, b.swt / b.sw));
    blocks.push_back(b);
    while (blocks.size() > 1 &&
           blocks[blocks.size() - 2].value > blocks.back().value) {
      auto right = blocks.back();
      blocks.pop_back();
      auto &left = blocks.back();
      left.end = right.end;
      left.sw += right.sw;
      left.swt += right.swt;
      left.lo = std::max(left.lo, right.lo);
      left.hi = std::min(left.hi, right.hi);
      if (left.lo > left.hi + 1e-10)
        throw std::invalid_argument("infeasible row bounds");
      left.value = std::max(left.lo, std::min(left.hi, left.swt / left.sw));
    }
  }
  std::vector<double> v(n);
  for (auto &b : blocks)
    for (size_t i = b.begin; i < b.end; ++i)
      v[i] = b.value + origin + prefix[i];
  return v;
}
inline std::pair<double, double> spacing_prox(double a, double b, double wa,
                                              double wb, double gap) {
  if (wa <= 0 || wb <= 0 || gap < 0)
    throw std::invalid_argument("spacing data");
  double d = std::max(0., gap - b + a);
  return {a - d * wb / (wa + wb), b + d * wa / (wa + wb)};
}
// mu * max(outside, max_i(abs(x_i-anchor_i)+dy_i)), with a declared peak cap.
inline std::vector<double>
peak_coordinate_prox(const std::vector<double> &t, const std::vector<double> &w,
                     const std::vector<double> &anchor,
                     const std::vector<double> &dy, double mu, double outside,
                     double cap) {
  size_t n = t.size();
  if (n != w.size() || n != anchor.size() || n != dy.size() || mu < 0)
    throw std::invalid_argument("peak dimensions");
  double lower = outside;
  std::vector<double> d(n);
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; ++i) {
    if (w[i] <= 0 || dy[i] < 0)
      throw std::invalid_argument("peak data");
    lower = std::max(lower, dy[i]);
    d[i] = std::abs(t[i] - anchor[i]) + dy[i];
    order[i] = i;
  }
  if (lower > cap)
    throw std::invalid_argument("peak infeasible cap");
  std::sort(order.begin(), order.end(), [&](size_t i, size_t j) {
    return std::make_pair(d[i], i) < std::make_pair(d[j], j);
  });
  size_t k = 0;
  std::vector<double> sw(n + 1), swd(n + 1);
  for (size_t j = n; j-- > 0;) {
    size_t i = order[j];
    sw[j] = sw[j + 1] + w[i];
    swd[j] = swd[j + 1] + w[i] * d[i];
  }
  while (k < n && d[order[k]] <= lower)
    ++k;
  double peak = lower;
  for (;;) {
    double root = k < n ? (swd[k] - mu) / sw[k] : lower;
    double next = k < n ? d[order[k]] : cap;
    if (root <= next || next >= cap || k == n || lower >= cap) {
      peak = std::max(lower, std::min(cap, root));
      break;
    }
    lower = next;
    while (k < n && d[order[k]] <= lower)
      ++k;
  }
  std::vector<double> v(n);
  for (size_t i = 0; i < n; ++i) {
    double r = peak - dy[i];
    v[i] = anchor[i] + std::max(-r, std::min(r, t[i] - anchor[i]));
  }
  return v;
}
inline double cell_coordinate_prox(double target, double weight, double alpha,
                                   double anchor, double lo, double hi,
                                   double quantum = 0) {
  if (weight <= 0 || alpha < 0 || lo > hi || quantum < 0)
    throw std::invalid_argument("cell data");
  double d = target - anchor;
  double x =
      anchor + std::copysign(std::max(0., std::abs(d) - alpha / weight), d);
  x = std::max(lo, std::min(hi, x));
  if (quantum == 0)
    return x;
  double l = std::max(lo, std::min(hi, std::floor(x / quantum) * quantum)),
         r = std::max(lo, std::min(hi, std::ceil(x / quantum) * quantum));
  auto f = [&](double v) {
    return alpha * std::abs(v - anchor) +
           .5 * weight * (v - target) * (v - target);
  };
  return f(l) <= f(r) ? l : r;
}
} // namespace dali::consensus_internal
