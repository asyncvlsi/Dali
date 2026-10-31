#pragma once
// Original in-repository numerical kernel; see ../README.md for provenance.
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dali::consensus_internal {

// Exact weighted proximal operator of mu * (max - min). Outside pins
// are immutable and have no quadratic penalty. Values use a common coordinate
// unit; each positive metric belongs to one pin agreement, not one net cost.
inline std::vector<double> range_prox(const std::vector<double> &t,
                                      const std::vector<double> &w, double mu,
                                      bool fixed = false, double fmin = 0,
                                      double fmax = 0) {
  if (t.size() != w.size() || mu < 0 || !std::isfinite(mu) ||
      (fixed && fmin > fmax))
    throw std::invalid_argument("range data");
  const size_t n = t.size();
  for (size_t i = 0; i < n; ++i)
    if (w[i] <= 0 || !std::isfinite(w[i]) || !std::isfinite(t[i]))
      throw std::invalid_argument("range metric");
  if (n == 0 || mu == 0)
    return t;
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; ++i)
    order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t i, size_t j) {
    return std::make_pair(t[i], i) < std::make_pair(t[j], j);
  });
  // Center coordinates before accumulation to avoid cancellation at large
  // absolute placement offsets. Lower and upper scans are independent.
  const double origin = t[order[0]];
  double sw = 0, swt = 0, a = 0, b = 0;
  for (size_t k = 0; k < n; ++k) {
    size_t i = order[k];
    sw += w[i];
    swt += w[i] * (t[i] - origin);
    a = (swt + mu) / sw;
    if (k + 1 == n || a <= t[order[k + 1]] - origin)
      break;
  }
  sw = 0;
  swt = 0;
  for (size_t k = n; k-- > 0;) {
    size_t i = order[k];
    sw += w[i];
    swt += w[i] * (t[i] - origin);
    b = (swt - mu) / sw;
    if (k == 0 || b >= t[order[k - 1]] - origin)
      break;
  }
  if (fixed) {
    a = std::min(a, fmin - origin);
    b = std::max(b, fmax - origin);
  }
  if (a > b) {
    sw = 0;
    swt = 0;
    for (size_t i = 0; i < n; ++i) {
      sw += w[i];
      swt += w[i] * (t[i] - origin);
    }
    a = b = swt / sw;
  }
  std::vector<double> v(n);
  for (size_t i = 0; i < n; ++i)
    v[i] = origin + std::max(a, std::min(b, t[i] - origin));
  return v;
}

// Exact local edge-pair minimization: physical penalty is paid once. Prices
// contain all quadratic one-hot terms. Bad is a rectangular binary table;
// hard overlapping combinations can have arbitrary cost because row factors
// forbid them in complete feasible assignments.
inline std::pair<int, int> edge_prox(const std::vector<double> &a,
                                     const std::vector<double> &b,
                                     const std::vector<unsigned char> &bad,
                                     double penalty) {
  if (a.empty() || b.empty() || bad.size() != a.size() * b.size() ||
      penalty < 0)
    throw std::invalid_argument("edge data");
  std::pair<int, int> best{-1, -1};
  double value = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < a.size(); ++i)
    for (size_t j = 0; j < b.size(); ++j) {
      double candidate = a[i] + b[j] + penalty * bool(bad[i * b.size() + j]);
      std::pair<int, int> key{int(i), int(j)};
      if (candidate < value ||
          (candidate == value && (best.first < 0 || key < best))) {
        best = key;
        value = candidate;
      }
    }
  return best;
}
} // namespace dali::consensus_internal
