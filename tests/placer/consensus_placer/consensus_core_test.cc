#include "dali/placer/consensus_placer/detail/solver.h"
#include <iomanip>
#include <iostream>
#include <random>
using namespace dali::consensus_internal;
static void check(bool b, const char *msg) {
  if (!b)
    throw std::runtime_error(msg);
}
static double exact(const Model &m) {
  std::vector<Site> x(m.cells.size());
  double best = INFINITY;
  std::function<void(int)> visit = [&](int i) {
    if (i == int(x.size())) {
      if (legal(m, x))
        best = std::min(best, hpwl(m, x) + displacement(m, x));
      return;
    }
    for (x[i] = m.cells[i].lo; x[i] <= m.cells[i].hi; ++x[i])
      visit(i + 1);
  };
  visit(0);
  return best;
}
int main(int argc, char **argv) {
  try {
    Options o;
    o.max_iterations = 1024;
    o.recover_every = 4;
    o.stagnant_checkpoints = 0;
    o.time_limit_seconds = 10;
    Model simple;
    simple.cells = {{2, 0, 20, 2, 0}, {12, 0, 20, 12, 0}};
    simple.rows = {{{0, 1}, {3}}};
    simple.nets = {{{{0, 0}}, {8}, 1}, {{{1, 0}}, {11}, 1}};
    auto r = refine(simple, o);
    check(legal(simple, r.x), "known legality");
    check(r.objective == exact(simple) && r.objective == 0,
          "known exact objective");
    // One tall physical owner appears in two rows; its L1 cost is charged once.
    Model tall;
    tall.cells = {{0, 0, 10, 0, .1}, {5, 0, 10, 5, .1}, {6, 0, 10, 6, .1}};
    tall.rows = {{{0, 1}, {2}}, {{0, 2}, {4}}};
    tall.nets = {{{{0, 0}}, {3}, 2}, {{{1, 0}, {2, .5}}, {}, 1}};
    auto t = refine(tall, o);
    double tall_exact = exact(tall);
    check(legal(tall, t.x) && t.row_copies == 4 && t.owners_considered == 3,
          "tall ownership");
    check(t.objective >= tall_exact - 1e-9 && t.objective <= t.source_objective,
          "tall objective");
    check(std::abs(t.displacement - displacement(tall, t.x)) < 1e-12,
          "physical cost once");
    std::mt19937 gen(918);
    int cases = 200, exact_matches = 0;
    double max_gap = 0, max_regression = 0;
    for (int z = 0; z < cases; ++z) {
      Model m;
      for (int i = 0; i < 3; ++i)
        m.cells.push_back(
            {Site(3 * i), 0, 8, double(gen() % 9), double(gen() % 3) * .15});
      m.rows = {{{0, 1, 2}, {1, 1}}, {{0, 2}, {2}}};
      for (int i = 0; i < 3; ++i)
        m.nets.push_back({{{i, double(gen() % 3) * .5}},
                          {double(gen() % 9)},
                          double(1 + gen() % 4)});
      m.nets.push_back({{{0, .5}, {1, 0}, {2, -.5}}, {}, double(gen() % 4)});
      auto q = refine(m, o);
      double optimum = exact(m);
      check(legal(m, q.x), "random recovery illegal");
      check(q.objective >= optimum - 1e-9 && q.objective <= q.source_objective,
            "random objective guard");
      max_gap = std::max(max_gap, q.objective - optimum);
      max_regression =
          std::max(max_regression, q.objective - q.source_objective);
      exact_matches += std::abs(q.objective - optimum) < 1e-8;
    }
    Options zero = o;
    zero.time_limit_seconds = 0;
    auto q = refine(tall, zero);
    check(q.x == std::vector<Site>({0, 5, 6}) && q.stop == "time_limit" &&
              q.iterations == 0,
          "zero budget source retention");
    zero = o;
    zero.max_iterations = 0;
    q = refine(tall, zero);
    check(q.iterations == 0 && q.accepted == 0, "zero iterations");
    zero = o;
    zero.cancelled = [] { return true; };
    q = refine(tall, zero);
    check(q.stop == "cancelled" && q.iterations == 0, "cancellation");
    int invalid = 0;
    auto reject = [&](Model m) {
      try {
        refine(m, o);
      } catch (std::invalid_argument const &) {
        ++invalid;
      }
    };
    auto bad = tall;
    bad.cells[0].source = 20;
    reject(bad);
    bad = tall;
    bad.rows[0].owners = {0, 0};
    reject(bad);
    bad = tall;
    bad.nets[0].pins[0].owner = 99;
    reject(bad);
    bad = tall;
    bad.nets[0].weight = NAN;
    reject(bad);
    bad = tall;
    bad.rows[0].separation = {8};
    reject(bad);
    check(invalid == 5, "invalid model rejection");
    double large_gain = 0, large_seconds = 0;
    std::size_t large_changed = 0;
    int large_iterations = 0;
    if (argc > 1 && std::string(argv[1]) == "--large") {
      Model large;
      int rows = 100, per = 100;
      for (int a = 0; a < rows; ++a) {
        Row row;
        for (int b = 0; b < per; ++b) {
          int i = a * per + b;
          large.cells.push_back({Site(b * 4), 0, 500, double(b * 4), 0});
          row.owners.push_back(i);
          if (b)
            row.separation.push_back(2);
          large.nets.push_back({{{i, 0}}, {double(b * 4 + 2)}, 1});
        }
        large.rows.push_back(row);
      }
      Options fast;
      fast.time_limit_seconds = .25;
      auto large_result = refine(large, fast);
      check(legal(large, large_result.x) &&
                large_result.objective < large_result.source_objective,
            "large usability");
      large_gain = large_result.source_objective - large_result.objective;
      large_seconds = large_result.seconds;
      large_changed = large_result.owners_changed;
      large_iterations = large_result.iterations;
    }
    std::cout << std::setprecision(17)
              << "{\"status\":\"PASS\",\"exact_reference_cases\":" << cases + 2
              << ",\"random_exact_matches\":" << exact_matches
              << ",\"random_max_objective_gap\":" << max_gap
              << ",\"maximum_source_regression\":" << max_regression
              << ",\"known_before\":" << r.source_objective
              << ",\"known_after\":" << r.objective
              << ",\"tall_before\":" << t.source_objective
              << ",\"tall_after\":" << t.objective
              << ",\"tall_exact\":" << tall_exact
              << ",\"invalid_inputs_rejected\":" << invalid
              << ",\"large_synthetic_gain\":" << large_gain
              << ",\"large_seconds\":" << large_seconds
              << ",\"large_changed\":" << large_changed
              << ",\"large_iterations\":" << large_iterations << "}\n";
  } catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
