/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 * SPDX-License-Identifier: GPL-2.0-or-later
 ******************************************************************************/
#include "consensus_cell_regions.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dali {
bool LoadConsensusCellRegions(Circuit &circuit, const std::string &path,
                              std::size_t *restored, std::string *error) {
  if (restored)
    *restored = 0;
  if (error)
    error->clear();
  auto require = [](bool ok, const char *reason) {
    if (!ok)
      throw std::invalid_argument(reason);
  };
  struct Wells {
    Macro *macro;
    std::vector<RectI> p, n;
  };
  std::vector<Wells> staged;
  try {
    require(std::isfinite(circuit.GridValueX()) && circuit.GridValueX() > 0 &&
                std::isfinite(circuit.GridValueY()) && circuit.GridValueY() > 0,
            "Invalid placement grid");
    std::ifstream input(path);
    require(bool(input), "Cannot read complete CELL input");
    auto site = [&](double um, double pitch) {
      const double x = um / pitch;
      require(std::isfinite(x) && x >= std::numeric_limits<int>::min() &&
                  x <= std::numeric_limits<int>::max() &&
                  std::abs(x - std::round(x)) <= 1e-7,
              "Off-grid or out-of-range CELL rectangle");
      return static_cast<int>(std::round(x));
    };
    std::set<std::string> names;
    std::string line, name;
    bool layer_n = false, have_layer = false;
    Wells current{};
    while (std::getline(input, line)) {
      std::istringstream words(line);
      std::string key, value;
      words >> key >> value;
      if (key == "MACRO") {
        require(name.empty() && !value.empty() && names.insert(value).second,
                "Nested or duplicate CELL master");
        require(circuit.IsMacroExisting(value), "Unknown CELL master");
        name = value;
        current = {circuit.GetMacroPtr(name), {}, {}};
        have_layer = false;
      } else if (!name.empty() && key == "LAYER") {
        require(value == "nwell" || value == "pwell", "Unsupported well layer");
        layer_n = value == "nwell";
        have_layer = true;
      } else if (!name.empty() && key == "RECT") {
        double lx, ly, ux, uy;
        std::istringstream rectangle(line);
        rectangle >> key >> lx >> ly >> ux >> uy;
        require(bool(rectangle) && have_layer, "Invalid CELL rectangle");
        RectI box(
            site(lx, circuit.GridValueX()), site(ly, circuit.GridValueY()),
            site(ux, circuit.GridValueX()), site(uy, circuit.GridValueY()));
        (layer_n ? current.n : current.p).push_back(box);
      } else if (!name.empty() && key == "END" && value == name) {
        require(!current.p.empty() && current.p.size() == current.n.size(),
                "Incomplete N/P region pairs");
        const int existing = current.macro->RegionCount();
        require(current.macro->HasCompleteWellRegions() &&
                    (existing == 1 ||
                     existing == static_cast<int>(current.p.size())),
                "Incompatible existing region count");
        int top = 0;
        bool previous_above = false;
        for (std::size_t r = 0; r < current.p.size(); ++r) {
          const auto &p = current.p[r];
          const auto &n = current.n[r];
          for (const auto *box : {&p, &n}) {
            require(box->LLX() >= 0 && box->LLY() >= 0 &&
                        box->LLX() < box->URX() && box->LLY() < box->URY() &&
                        box->URX() <= current.macro->Width() &&
                        box->URY() <= current.macro->Height(),
                    "Well rectangle outside master");
          }
          const bool above = p.URY() == n.LLY();
          require(above || n.URY() == p.LLY(), "Nonabutting N/P pair");
          require(std::min(p.LLY(), n.LLY()) == top &&
                      (r == 0 || above != previous_above),
                  "Nonabutting or inconsistent region sequence");
          previous_above = above;
          top = std::max(p.URY(), n.URY());
        }
        require(top == current.macro->Height(), "Well/master height mismatch");
        staged.push_back(std::move(current));
        name.clear();
      }
    }
    require(input.eof() && name.empty() && !staged.empty(),
            "Incomplete CELL input");
    std::size_t count = 0;
    for (auto &wells : staged) {
      const auto existing =
          static_cast<std::size_t>(wells.macro->RegionCount());
      for (std::size_t r = 0; r < wells.p.size(); ++r) {
        const auto &p = wells.p[r];
        const auto &n = wells.n[r];
        if (r < existing) {
          wells.macro->PwellRect(static_cast<int>(r)) = p;
          wells.macro->NwellRect(static_cast<int>(r)) = n;
        } else {
          wells.macro->AddPwellRect(p.LLX(), p.LLY(), p.URX(), p.URY());
          wells.macro->AddNwellRect(n.LLX(), n.LLY(), n.URX(), n.URY());
        }
      }
      count += wells.p.size() > 1;
    }
    if (restored)
      *restored = count;
    return true;
  } catch (const std::exception &exception) {
    if (error)
      *error = exception.what();
    return false;
  }
}
} // namespace dali
