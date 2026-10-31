// Copyright (c) 2026 Yihang Yang
// SPDX-License-Identifier: GPL-2.0-or-later
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using Exact = boost::multiprecision::cpp_dec_float_50;
struct Net {
  std::string name;
  double weight, lo = INFINITY, hi = -INFINITY;
};
int main(int argc, char** argv) {
  try {
    if (argc != 2)
      throw std::runtime_error(
          "usage: consensus_check_objective placement.multi_region.source");
    std::ifstream in(argv[1]);
    if (!in) throw std::runtime_error("cannot read state");
    std::string line;
    double pitch = 0;
    std::vector<Net> nets;
    while (std::getline(in, line)) {
      std::istringstream words(line);
      std::string tag;
      words >> tag;
      if (tag == "GRID")
        words >> pitch;
      else if (tag == "NET") {
        std::string name;
        double weight;
        words >> std::quoted(name) >> weight;
        nets.push_back({name, weight});
      } else if (tag == "PIN") {
        std::string name, owner;
        double ox, oy, x, y;
        words >> std::quoted(name) >> std::quoted(owner) >> ox >> oy >> x >> y;
        if (!words || nets.empty() || nets.back().name != name)
          throw std::runtime_error("bad pin stream");
        nets.back().lo = std::min(nets.back().lo, x);
        nets.back().hi = std::max(nets.back().hi, x);
      }
    }
    if (!in.eof() || pitch <= 0) throw std::runtime_error("incomplete state");
    double grid_sum = 0, scaled_sum = 0;
    Exact reference = 0;
    for (auto net : nets)
      if (net.lo <= net.hi) {
        grid_sum += net.weight * (net.hi - net.lo);
        scaled_sum += (net.weight * pitch) * (net.hi - net.lo);
        reference +=
            Exact(net.weight) * (Exact(net.hi) - Exact(net.lo)) * Exact(pitch);
      }
    const double native = grid_sum * pitch;
    const double eps = std::numeric_limits<double>::epsilon();
    const double neps = (nets.size() + 2.) * eps;
    const double bound = (2 * neps / (1 - neps) + 8 * eps) *
                         std::max({1., std::abs(native), std::abs(scaled_sum)});
    std::cout << std::setprecision(17) << "{\"nets\":" << nets.size()
              << ",\"native_grid_then_scale_um\":" << native
              << ",\"model_scale_then_sum_um\":" << scaled_sum
              << ",\"difference_um\":" << std::abs(native - scaled_sum)
              << ",\"roundoff_bound_um\":" << bound
              << ",\"reference_digits\":50,\"reference_um\":\""
              << std::setprecision(50) << reference
              << "\",\"native_error_um\":\"" << abs(Exact(native) - reference)
              << "\",\"model_error_um\":\""
              << abs(Exact(scaled_sum) - reference) << "\"}\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
