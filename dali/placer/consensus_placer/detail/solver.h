#pragma once

// Fixed-assignment horizontal consensus. Coordinate unit: one common grid site.
// Kernels are the preserved Dali research implementations, not external code.
#include "coordinate_prox.h"
#include "coupled_prox.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <queue>
#include <set>
#include <string>

namespace dali::consensus_internal {
using Site = std::int64_t;
struct Cell {
  Site source = 0, lo = 0, hi = 0;
  double anchor = 0, displacement_weight = 0;
};
struct Row {
  std::vector<int> owners;
  // Minimum x[k+1]-x[k], including width and required spacing.
  std::vector<Site> separation;
};
struct Pin {
  int owner = -1;
  double offset = 0;
};
struct Net {
  std::vector<Pin> pins;
  // Absolute fixed pin coordinates; any number, including none.
  std::vector<double> fixed_pins;
  double weight = 1;
};
struct Model {
  std::vector<Cell> cells;
  std::vector<Row> rows;
  std::vector<Net> nets;
};
struct Options {
  int max_iterations = 128, recover_every = 8, stagnant_checkpoints = 4;
  double time_limit_seconds = 1, rho = 1;
  double primal_tolerance = 1e-5, dual_tolerance = 1e-5;
  double minimum_improvement = 1e-9;
  // Invoked only at safe boundaries; true requests cancellation.
  std::function<bool()> cancelled;
};
struct Result {
  std::vector<Site> x;
  double source_objective = 0, objective = 0;
  double source_hpwl = 0, hpwl = 0, displacement = 0;
  double primal_residual = 0, dual_residual = 0, seconds = 0;
  std::size_t owners_considered = 0, owners_changed = 0, row_copies = 0,
              pin_copies = 0;
  int iterations = 0, recoveries = 0, accepted = 0;
  std::string stop;
};
inline void require(bool b, const char *why) {
  if (!b)
    throw std::invalid_argument(why);
}
inline bool finite(double v) { return std::isfinite(v); }
inline double hpwl(const Model &m, const std::vector<Site> &x) {
  double total = 0;
  for (const auto &net : m.nets) {
    double lo = INFINITY, hi = -INFINITY;
    for (double p : net.fixed_pins) {
      lo = std::min(lo, p);
      hi = std::max(hi, p);
    }
    for (auto p : net.pins) {
      double v = double(x[p.owner]) + p.offset;
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    if (lo <= hi)
      total += net.weight * (hi - lo);
  }
  return total;
}
inline double displacement(const Model &m, const std::vector<Site> &x) {
  double total = 0;
  for (std::size_t i = 0; i < x.size(); ++i)
    total += m.cells[i].displacement_weight *
             std::abs(double(x[i]) - m.cells[i].anchor);
  return total;
}
inline bool legal(const Model &m, const std::vector<Site> &x) {
  if (x.size() != m.cells.size())
    return false;
  for (std::size_t i = 0; i < x.size(); ++i)
    if (x[i] < m.cells[i].lo || x[i] > m.cells[i].hi)
      return false;
  for (const auto &row : m.rows)
    for (std::size_t k = 1; k < row.owners.size(); ++k)
      if (x[row.owners[k]] - x[row.owners[k - 1]] < row.separation[k - 1])
        return false;
  return true;
}

inline Result refine(const Model &m, const Options &opt = {}) {
  using Clock = std::chrono::steady_clock;
  const auto started = Clock::now();
  auto seconds = [&] {
    return std::chrono::duration<double>(Clock::now() - started).count();
  };
  require(opt.max_iterations >= 0 && opt.recover_every > 0 &&
              opt.stagnant_checkpoints >= 0,
          "invalid iteration/stagnation controls");
  require(finite(opt.time_limit_seconds) && opt.time_limit_seconds >= 0 &&
              finite(opt.rho) && opt.rho > 0,
          "invalid time/rho controls");
  require(finite(opt.primal_tolerance) && opt.primal_tolerance >= 0 &&
              finite(opt.dual_tolerance) && opt.dual_tolerance >= 0 &&
              finite(opt.minimum_improvement) && opt.minimum_improvement >= 0,
          "invalid tolerance");
  require(m.cells.size() <= std::size_t(INT32_MAX), "too many owners");
  const int n = int(m.cells.size());
  std::vector<Site> source(n), upper(n);
  std::vector<double> x(n), memberships(n), incidence(n), metric(n);
  std::vector<std::vector<std::pair<int, Site>>> outgoing(n);
  std::vector<int> indegree(n), order;
  for (int i = 0; i < n; ++i) {
    const auto &c = m.cells[i];
    require(c.lo >= -(Site(1) << 40) && c.hi <= (Site(1) << 40) &&
                c.lo <= c.source && c.source <= c.hi,
            "invalid source or safe site bounds");
    require(finite(c.anchor) && std::abs(c.anchor) <= double(Site(1) << 40) &&
                finite(c.displacement_weight) && c.displacement_weight >= 0,
            "invalid cell cost");
    source[i] = c.source;
    upper[i] = c.hi;
    x[i] = double(c.source);
  }
  for (const auto &row : m.rows) {
    require(row.separation.size() ==
                (row.owners.empty() ? 0 : row.owners.size() - 1),
            "row dimensions");
    std::set<int> seen;
    for (int i : row.owners) {
      require(i >= 0 && i < n && seen.insert(i).second,
              "invalid/duplicate row owner");
      ++memberships[i];
    }
    for (std::size_t k = 1; k < row.owners.size(); ++k) {
      Site gap = row.separation[k - 1];
      require(gap > 0 && gap <= (Site(1) << 40), "invalid row separation");
      int a = row.owners[k - 1], b = row.owners[k];
      outgoing[a].push_back({b, gap});
      ++indegree[b];
    }
  }
  for (const auto &net : m.nets) {
    require(finite(net.weight) && net.weight >= 0, "invalid net weight");
    for (auto p : net.pins) {
      require(p.owner >= 0 && p.owner < n && finite(p.offset) &&
                  std::abs(p.offset) <= double(Site(1) << 40),
              "invalid net pin");
      if (net.weight > 0)
        ++incidence[p.owner];
    }
    for (double p : net.fixed_pins)
      require(finite(p) && std::abs(p) <= double(Site(1) << 40),
              "invalid fixed pin");
  }
  require(legal(m, source), "source must already be legal");
  std::queue<int> ready;
  for (int i = 0; i < n; ++i)
    if (!indegree[i])
      ready.push(i);
  while (!ready.empty()) {
    int a = ready.front();
    ready.pop();
    order.push_back(a);
    for (auto e : outgoing[a])
      if (!--indegree[e.first])
        ready.push(e.first);
  }
  require(order.size() == std::size_t(n), "inconsistent cyclic row order");
  for (auto it = order.rbegin(); it != order.rend(); ++it)
    for (auto e : outgoing[*it])
      upper[*it] = std::min(upper[*it], upper[e.first] - e.second);
  struct Copy {
    std::vector<double> v, u, w;
  };
  std::vector<Copy> rows, nets;
  Result result;
  result.x = source;
  result.owners_considered = n;
  result.source_hpwl = result.hpwl = hpwl(m, source);
  result.displacement = displacement(m, source);
  result.source_objective = result.objective =
      result.hpwl + result.displacement;
  require(finite(result.objective), "objective overflow");
  for (const auto &row : m.rows) {
    Copy c;
    for (int i : row.owners) {
      c.v.push_back(x[i]);
      c.u.push_back(0);
      c.w.push_back(1 / memberships[i]);
      metric[i] += c.w.back();
    }
    result.row_copies += c.v.size();
    rows.push_back(std::move(c));
  }
  for (const auto &net : m.nets) {
    Copy c;
    for (auto p : net.pins) {
      double w = net.weight > 0 ? 1 / incidence[p.owner] : 0;
      c.v.push_back(x[p.owner] + p.offset);
      c.u.push_back(0);
      c.w.push_back(w);
      metric[p.owner] += w;
    }
    result.pin_copies += c.v.size();
    nets.push_back(std::move(c));
  }
  for (double &w : metric)
    if (w == 0)
      w = 1; // isolated owners still have a valid proximal metric
  auto interrupted = [&] {
    if (opt.cancelled && opt.cancelled()) {
      result.stop = "cancelled";
      return true;
    }
    if (seconds() >= opt.time_limit_seconds) {
      result.stop = "time_limit";
      return true;
    }
    return false;
  };
  auto recover = [&] {
    // A single shared integer vector satisfies EVERY occupancy row. Backward
    // upper propagation guarantees the forward repair always has room.
    std::vector<Site> lower(n), candidate(n);
    for (int i = 0; i < n; ++i)
      lower[i] = m.cells[i].lo;
    for (int i : order) {
      require(finite(x[i]), "nonfinite consensus coordinate");
      Site target = Site(std::floor(
          std::max(double(m.cells[i].lo), std::min(double(upper[i]), x[i])) +
          .5));
      candidate[i] = std::max(lower[i], std::min(upper[i], target));
      for (auto e : outgoing[i])
        lower[e.first] = std::max(lower[e.first], candidate[i] + e.second);
    }
    require(legal(m, candidate), "internal legal recovery failed");
    ++result.recoveries;
    double wire = hpwl(m, candidate), disp = displacement(m, candidate),
           cost = wire + disp;
    require(finite(cost), "candidate objective overflow");
    if (cost < result.objective - opt.minimum_improvement) {
      result.x = std::move(candidate);
      result.objective = cost;
      result.hpwl = wire;
      result.displacement = disp;
      ++result.accepted;
      return true;
    }
    return false;
  };
  int stagnant = 0;
  result.stop = "iteration_limit";
  for (int iteration = 1; iteration <= opt.max_iterations; ++iteration) {
    if (interrupted())
      break;
    auto old = x;
    bool aborted = false;
    for (std::size_t r = 0; r < rows.size(); ++r) {
      if (interrupted()) {
        aborted = true;
        break;
      }
      auto &copy = rows[r];
      const auto &row = m.rows[r];
      std::vector<double> target, lo, hi, width;
      for (std::size_t k = 0; k < row.owners.size(); ++k) {
        int i = row.owners[k];
        target.push_back(x[i] - copy.u[k]);
        lo.push_back(double(m.cells[i].lo));
        hi.push_back(double(m.cells[i].hi));
        width.push_back(k < row.separation.size() ? double(row.separation[k])
                                                  : 0);
      }
      copy.v = row_coordinate_prox(target, copy.w, lo, hi, width);
    }
    if (aborted)
      break;
    for (std::size_t j = 0; j < nets.size(); ++j) {
      if (interrupted()) {
        aborted = true;
        break;
      }
      auto &copy = nets[j];
      const auto &net = m.nets[j];
      if (net.weight == 0)
        continue;
      std::vector<double> target, weights;
      for (std::size_t k = 0; k < net.pins.size(); ++k) {
        auto p = net.pins[k];
        target.push_back(x[p.owner] + p.offset - copy.u[k]);
        weights.push_back(opt.rho * copy.w[k]);
      }
      bool fixed = !net.fixed_pins.empty();
      double low = 0, high = 0;
      if (fixed) {
        auto bounds =
            std::minmax_element(net.fixed_pins.begin(), net.fixed_pins.end());
        low = *bounds.first;
        high = *bounds.second;
      }
      copy.v = range_prox(target, weights, net.weight, fixed, low, high);
    }
    if (aborted)
      break;
    std::vector<double> sum(n), mass(n);
    for (std::size_t r = 0; r < rows.size(); ++r)
      for (std::size_t k = 0; k < rows[r].v.size(); ++k) {
        int i = m.rows[r].owners[k];
        const auto &c = rows[r];
        sum[i] += c.w[k] * (c.v[k] + c.u[k]);
        mass[i] += c.w[k];
      }
    for (std::size_t j = 0; j < nets.size(); ++j)
      for (std::size_t k = 0; k < nets[j].v.size(); ++k) {
        auto p = m.nets[j].pins[k];
        const auto &c = nets[j];
        sum[p.owner] += c.w[k] * (c.v[k] + c.u[k] - p.offset);
        mass[p.owner] += c.w[k];
      }
    result.dual_residual = 0;
    for (int i = 0; i < n; ++i) {
      const auto &c = m.cells[i];
      double target = mass[i] ? sum[i] / mass[i] : old[i];
      x[i] = cell_coordinate_prox(target, opt.rho * metric[i],
                                  c.displacement_weight, c.anchor, double(c.lo),
                                  double(c.hi));
      require(finite(x[i]), "nonfinite owner update");
      result.dual_residual = std::max(
          result.dual_residual, opt.rho * metric[i] * std::abs(x[i] - old[i]));
    }
    result.primal_residual = 0;
    for (std::size_t r = 0; r < rows.size(); ++r)
      for (std::size_t k = 0; k < rows[r].v.size(); ++k) {
        auto &c = rows[r];
        double d = c.v[k] - x[m.rows[r].owners[k]];
        c.u[k] += d;
        result.primal_residual = std::max(result.primal_residual, std::abs(d));
      }
    for (std::size_t j = 0; j < nets.size(); ++j)
      if (m.nets[j].weight > 0)
        for (std::size_t k = 0; k < nets[j].v.size(); ++k) {
          auto &c = nets[j];
          auto p = m.nets[j].pins[k];
          double d = c.v[k] - x[p.owner] - p.offset;
          c.u[k] += d;
          result.primal_residual =
              std::max(result.primal_residual, std::abs(d));
        }
    result.iterations = iteration;
    bool converged = result.primal_residual <= opt.primal_tolerance &&
                     result.dual_residual <= opt.dual_tolerance;
    if (iteration % opt.recover_every == 0 || converged ||
        iteration == opt.max_iterations) {
      if (interrupted())
        break;
      stagnant = recover() ? 0 : stagnant + 1;
      if (converged) {
        result.stop = "residual_tolerance";
        break;
      }
      if (opt.stagnant_checkpoints && stagnant >= opt.stagnant_checkpoints) {
        result.stop = "stagnation";
        break;
      }
    }
  }
  require(legal(m, result.x) && result.objective <= result.source_objective,
          "retained result guard");
  for (int i = 0; i < n; ++i)
    result.owners_changed += result.x[i] != source[i];
  result.seconds = seconds();
  return result;
}
} // namespace dali::consensus_internal
