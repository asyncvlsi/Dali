// Copyright (c) 2026 Yihang Yang
// SPDX-License-Identifier: GPL-2.0-or-later
// Public connectivity -> explicitly derived P/N-well engineering abstracts.
#include <phydb/phydb.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include "dali/circuit/circuit.h"

namespace fs = std::filesystem;
void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
  try {
    require(argc == 6,
            "usage: consensus_prepare_public source.lef source.def "
            "output-directory single|mixed whitespace-density");
    const std::string mode = argv[4];
    require(mode == "single" || mode == "mixed", "invalid height mode");
    std::size_t used = 0;
    const double density = std::stod(argv[5], &used);
    require(used == std::string(argv[5]).size() && std::isfinite(density) &&
                density > 0 && density <= 1,
            "invalid density");
    fs::path output = argv[3];
    require(!fs::exists(output),
            "refusing to overwrite an existing derived case");
    phydb::PhyDB db;
    db.ReadLef(argv[1]);
    db.ReadDef(argv[2]);
    auto& sites = db.GetTechPtr()->GetSitesRef();
    require(!sites.empty(), "source needs a LEF SITE");
    const double gx = sites.front().GetWidth(),
                 base_um = sites.front().GetHeight(), gy = base_um / 10;
    require(gx > 0 && gy > 0, "invalid source SITE");
    dali::Circuit c;
    db.SetPlacementGrids(gx, gy);
    c.InitializeFromPhyDB(&db);
    std::map<std::string, dali::Macro*> ordinary;
    std::map<dali::Macro*, int> classes;
    std::map<int, std::size_t> class_counts;
    std::size_t owners = 0, fixed = 0, io = 0, edges = 0;
    long double area = 0, fixed_area = 0;
    std::set<dali::Macro*> fixed_masters;
    int widest = 0, tallest = 0;
    for (auto& cell : c.Components()) {
      if (cell.MacroPtr() == c.tech().IoDummyMacroPtr()) {
        ++io;
        continue;
      }
      ordinary.emplace(cell.MacroPtr()->Name(), cell.MacroPtr());
      if (!cell.IsMovable()) {
        require(cell.Orient() == dali::N,
                "non-N fixed macro requires an explicit transformation policy");
        ++fixed;
        fixed_area +=
            static_cast<long double>(cell.Width()) * cell.MacroPtr()->Height();
        fixed_masters.insert(cell.MacroPtr());
        widest = std::max(widest, cell.Width());
        tallest = std::max(tallest, cell.MacroPtr()->Height() + 4);
        continue;
      }
      double decks = cell.MacroPtr()->Height() / 10.;
      int count = mode == "single" ? 1 : int(std::llround(decks));
      require(std::abs(decks - std::round(decks)) < 1e-8 && count >= 1 &&
                  count <= 8,
              "nonintegral/unsupported source height class");
      classes[cell.MacroPtr()] = count;
      ++owners;
      ++class_counts[count];
      area += static_cast<long double>(cell.Width()) * 10 * count;
      widest = std::max(widest, cell.Width());
      tallest = std::max(tallest, 12 * count + 4);
    }
    for (auto& pin : c.design().IoPins())
      require(pin.IsPrePlaced(), "unplaced IO needs an explicit anchor policy");
    require(owners > 0, "source has no movable cells");
    for (auto* m : fixed_masters)
      require(!classes.count(m),
              "fixed/movable shared master needs separate cloning policy");
    const double target_area = double(area) / density + double(fixed_area);
    const double aspect =
        std::max(0.01, c.RegionWidth() * gx / (c.RegionHeight() * gy));
    int width = std::max(
        widest + 16, int(std::ceil(std::sqrt(target_area * gy / gx * aspect))));
    int height = std::max(tallest, int(std::ceil(target_area / width)));
    const double max_plug = std::ceil(std::hypot(width * gx, height * gy)) + 1;
    require(!c.IsMacroExisting("__dali_derived_tap"),
            "derived tap name collides");
    auto xsite = [&](double x) {
      return std::clamp(
          std::llround((x - c.RegionLLX()) / c.RegionWidth() * width), 0LL,
          (long long)width);
    };
    auto ysite = [&](double y) {
      return std::clamp(
          std::llround((y - c.RegionLLY()) / c.RegionHeight() * height), 0LL,
          (long long)height);
    };
    // Affine relocation retains fixed dimensions. Shrinking the frame can
    // therefore make originally disjoint hard macros overlap; reject that
    // converted task before creating any output or running a legalizer.
    struct FixedBox {
      std::string name;
      long long x, y, w, h;
    };
    std::vector<FixedBox> boxes;
    for (auto& cell : c.Components()) {
      if (cell.IsMovable() || cell.Area() == 0) continue;
      const auto x = std::clamp(xsite(cell.LLX()), 0LL,
                               (long long)width - cell.Width());
      const auto y = std::clamp(ysite(cell.LLY()), 0LL,
                               (long long)height - cell.MacroPtr()->Height());
      FixedBox box{cell.Name(), x, y, cell.Width(), cell.MacroPtr()->Height()};
      for (const auto& old : boxes)
        if (std::min(box.x + box.w, old.x + old.w) > std::max(box.x, old.x) &&
            std::min(box.y + box.h, old.y + old.h) > std::max(box.y, old.y))
          throw std::runtime_error("derived fixed macros overlap: " + old.name +
                                   " and " + box.name +
                                   "; requested density/mapping is invalid");
      boxes.push_back(box);
    }
    fs::create_directories(output);
    std::ofstream lef(output / "cells.lef"), cellfile(output / "cells.cell"),
        def(output / "input.def"), map(output / "owners.tsv"),
        pins(output / "connectivity.tsv"), meta(output / "conversion.json");
    require(lef && cellfile && def && map && pins && meta,
            "cannot create derived output");
    lef << std::fixed << std::setprecision(9)
        << "VERSION 5.8 ;\nBUSBITCHARS \"[]\" ;\nDIVIDERCHAR \"/\" ;\nUNITS\n "
           "DATABASE MICRONS 1000 ;\nEND UNITS\nMANUFACTURINGGRID 0.001 ;\n"
        << "SITE DerivedGrid\n CLASS CORE ;\n SYMMETRY X Y ;\n SIZE " << gx
        << " BY " << gy << " ;\nEND DerivedGrid\n";
    for (auto layer :
         {std::make_pair("M1", "HORIZONTAL"), std::make_pair("M2", "VERTICAL")})
      lef << "LAYER " << layer.first << "\n TYPE ROUTING ;\n DIRECTION "
          << layer.second << " ;\n PITCH " << gx << " " << gy
          << " ;\n WIDTH 0.01 ;\n SPACING 0.01 ;\nEND " << layer.first << "\n";
    cellfile << std::fixed << std::setprecision(9)
             << "LAYER LEGALIZER\n SAME_DIFF_SPACING 0 ;\n ANY_DIFF_SPACING 0 "
                ";\n WELLTAP_ADJUST 0 ;\nEND LEGALIZER\n";
    for (auto name : {"pwell", "nwell"})
      cellfile << "LAYER " << name << "\n MINWIDTH " << std::min(gx, gy)
               << " ;\n SPACING " << gx << " ;\n OPPOSPACING 0 ;\n MAXPLUGDIST "
               << max_plug << " ;\n OVERHANG 0 ;\nEND " << name << "\n";
    for (auto [master_name, m] : ordinary) {
      const bool is_fixed = fixed_masters.count(m);
      int count = is_fixed ? 0 : classes.at(m);
      double height_um = is_fixed ? m->Height() * gy : count * 10 * gy;
      lef << "MACRO " << m->Name() << "\n CLASS CORE ;\n ORIGIN 0 0 ;\n SIZE "
          << m->Width() * gx << " BY " << height_um
          << " ;\n SYMMETRY X Y ;\n SITE DerivedGrid ;\n";
      for (auto& p : m->PinList()) {
        const double x = p.OffsetX() * gx,
                     y = p.OffsetY() * gy *
                         (is_fixed ? 1. : count * 10. / m->Height()),
                     e = std::min(gx, gy) * .01;
        lef << " PIN " << p.Name() << "\n  DIRECTION "
            << (p.IsInput() ? "INPUT" : "OUTPUT")
            << " ;\n  USE SIGNAL ;\n  PORT\n   LAYER M1 ;\n   RECT " << x - e
            << " " << y - e << " " << x + e << " " << y + e
            << " ;\n  END\n END " << p.Name() << "\n";
      }
      lef << "END " << m->Name() << "\n";
      if (is_fixed) continue;
      cellfile << "MACRO " << m->Name() << "\n VERSION " << m->Name()
               << "\n UNPLUG\n";
      for (int r = 0; r < count; ++r) {
        const int base = 10 * r;
        for (bool n : {false, true}) {
          const int lower =
              r % 2 ? (n ? base : base + 6) : (n ? base + 4 : base);
          const int upper =
              r % 2 ? (n ? base + 6 : base + 10) : (n ? base + 10 : base + 4);
          cellfile << " LAYER " << (n ? "nwell" : "pwell") << " ;\n RECT 0 "
                   << lower * gy << " " << m->Width() * gx << " " << upper * gy
                   << " ;\n END\n";
        }
      }
      cellfile << " END VERSION\nEND " << m->Name() << "\n";
    }
    lef << "MACRO __dali_derived_tap\n CLASS CORE WELLTAP ;\n ORIGIN 0 0 ;\n "
           "SIZE "
        << 2 * gx << " BY " << 12 * gy
        << " ;\n SYMMETRY X Y ;\n SITE DerivedGrid ;\nEND "
           "__dali_derived_tap\nEND LIBRARY\n";
    cellfile << "MACRO __dali_derived_tap\n VERSION __dali_derived_tap\n "
                "PLUG\n LAYER pwell ;\n RECT 0 0 "
             << 2 * gx << " " << 5 * gy << " ;\n END\n LAYER nwell ;\n RECT 0 "
             << 5 * gy << " " << 2 * gx << " " << 12 * gy
             << " ;\n END\n END VERSION\nEND __dali_derived_tap\n";
    auto dx = [&](long long x) { return std::llround(x * gx * 1000); };
    auto dy = [&](long long y) { return std::llround(y * gy * 1000); };
    def << "VERSION 5.8 ;\nDIVIDERCHAR \"/\" ;\nBUSBITCHARS \"[]\" ;\nDESIGN "
           "derived_public ;\nUNITS DISTANCE MICRONS 1000 ;\nDIEAREA ( 0 0 ) ( "
        << dx(width) << " " << dy(height) << " ) ;\nCOMPONENTS "
        << owners + fixed << " ;\n";
    map << "owner\tmaster\tregions\twidth_sites\tsource_height_sites\tderived_"
           "height_sites\tsource_x_um\tsource_y_um\tderived_x_sites\tderived_y_"
           "sites\n";
    for (auto& p : c.Components()) {
      if (p.MacroPtr() == c.tech().IoDummyMacroPtr()) continue;
      int count = p.IsMovable() ? classes.at(p.MacroPtr()) : 0;
      int target_height = count ? 10 * count : p.MacroPtr()->Height();
      auto x = std::clamp(xsite(p.LLX()), 0LL, (long long)width - p.Width());
      auto y =
          std::clamp(ysite(p.LLY()), 0LL, (long long)height - target_height);
      def << "- " << p.Name() << " " << p.MacroPtr()->Name()
          << (p.IsMovable() ? " + PLACED ( " : " + FIXED ( ") << dx(x) << " "
          << dy(y) << " ) N ;\n";
      map << p.Name() << "\t" << p.MacroPtr()->Name() << "\t" << count << "\t"
          << p.Width() << "\t" << p.MacroPtr()->Height() << "\t"
          << target_height << "\t" << std::setprecision(17) << p.LLX() * gx
          << "\t" << p.LLY() * gy << "\t" << x << "\t" << y << "\n";
    }
    def << "END COMPONENTS\nPINS " << c.design().IoPins().size() << " ;\n";
    for (auto& pin : c.design().IoPins())
      def << "- " << pin.Name() << " + NET " << pin.NetName() << " + DIRECTION "
          << pin.SigDirectStr()
          << " + USE SIGNAL\n + LAYER M1 ( -1 -1 ) ( 1 1 )\n + FIXED ( "
          << dx(xsite(pin.X())) << " " << dy(ysite(pin.Y())) << " ) N ;\n";
    def << "END PINS\nNETS " << c.Nets().size() << " ;\n";
    pins << "net\tkind\towner\tpin\n";
    for (auto& n : c.Nets()) {
      def << "- " << n.Name();
      for (auto& p : n.ComponentPins()) {
        if (p.ComponentPtr()->MacroPtr() == c.tech().IoDummyMacroPtr())
          continue;
        def << " ( " << p.ComponentPtr()->Name() << " " << p.PinName() << " )";
        pins << n.Name() << "\tcomponent\t" << p.ComponentPtr()->Name() << "\t"
             << p.PinName() << "\n";
        ++edges;
      }
      for (auto* p : n.IoPinPtrs()) {
        def << " ( PIN " << p->Name() << " )";
        pins << n.Name() << "\tio\t" << p->Name() << "\tPIN\n";
        ++edges;
      }
      def << " ;\n";
    }
    def << "END NETS\nEND DESIGN\n";
    meta << std::setprecision(17)
         << "{\n  \"schema\": 1,\n  \"source_kind\": \"public-derived "
            "engineering abstract; not original contest geometry\",\n"
         << "  \"height_mode\": \"" << mode << "\",\n  \"owners\": " << owners
         << ",\n  \"io_anchors\": " << io
         << ",\n  \"nets\": " << c.Nets().size()
         << ",\n  \"connections\": " << edges << ",\n"
         << "  \"pitch_x_um\": " << gx << ",\n  \"pitch_y_um\": " << gy
         << ",\n  \"width_sites\": " << width
         << ",\n  \"height_sites\": " << height << ",\n"
         << "  \"raw_area_sites\": " << double(area)
         << ",\n  \"requested_density\": " << density
         << ",\n  \"actual_raw_density\": "
         << double(area) / (width * double(height))
         << ",\n  \"max_plug_dist_um\": " << max_plug
         << ",\n  \"height_counts\": {";
    bool first = true;
    for (auto [count, number] : class_counts) {
      if (!first) meta << ", ";
      first = false;
      meta << "\"" << count << "\": " << number;
    }
    meta << "},\n  \"fixed_physical_macros\": " << fixed
         << ",\n  \"fixed_area_sites\": " << double(fixed_area)
         << ",\n  \"actual_whitespace_density\": "
         << double(area) / (width * double(height) - double(fixed_area))
         << "\n}\n";
    for (auto* stream : {&lef, &cellfile, &def, &map, &pins, &meta}) {
      stream->flush();
      require(bool(*stream), "derived output write failed");
    }
    std::cout << "Prepared " << owners << " owners, " << c.Nets().size()
              << " nets at raw density "
              << double(area) / (width * double(height)) << "\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
