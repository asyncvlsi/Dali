/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/

/**
 * @file
 * Exercise `place-environment`, which fixes off-chip environment cells just
 * outside an edge of the placement region.
 *
 * Checks the geometry the timing flow relies on: each stack is flush against
 * its edge and outside the region, contiguous in the order given, centred on
 * the edge, and FIXED. Then checks that every malformed request fails without
 * moving anything: an unknown edge, a reused edge, an unknown, repeated or
 * already fixed component. Finally, a stack longer than its edge: alone it
 * must fail and place nothing; given a second edge it must fill the first
 * contiguously and continue on the second.
 */

#include <cstdio>
#include <string>
#include <vector>

#include "dali/dali.h"

using namespace dali;

namespace {

int failures = 0;

void Expect(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "FAILED: %s\n", what);
    ++failures;
  }
}

}  // namespace

int main() {
  phydb::PhyDB phy_db;
  phy_db.ReadLef("ispd19_test3.input.lef");
  phy_db.ReadDef("ispd19_test3.input.def");

  std::vector<std::string> names;
  auto &phydb_components = phy_db.design().GetComponentsRef();
  for (auto it = phydb_components.rbegin();
       it != phydb_components.rend() && names.size() < 5; ++it) {
    if (it->GetPlacementStatus() != phydb::PlaceStatus::FIXED) {
      names.push_back(it->GetName());
    }
  }
  if (names.size() < 5) {
    std::fprintf(stderr, "design has fewer than 5 movable components\n");
    return 1;
  }

  Dali dali(&phy_db, severity::info);
  Expect(!dali.ExecuteCommand({"place-environment", "left", names[0]}),
         "unknown edge is rejected");
  Expect(!dali.ExecuteCommand({"place-environment", "west", "no_such_cell"}),
         "unknown component is rejected");
  Expect(!dali.ExecuteCommand(
             {"place-environment", "west", names[0], names[0]}),
         "repeated component is rejected");

  Circuit &circuit = dali.GetCircuit();
  Component *a = circuit.GetComponentPtr(names[0]);
  Component *b = circuit.GetComponentPtr(names[1]);
  Component *c = circuit.GetComponentPtr(names[2]);
  Expect(a->IsMovable() && b->IsMovable() && c->IsMovable(),
         "failed requests left the cells movable");

  Expect(dali.ExecuteCommand(
             {"place-environment", "west", names[0], names[1], names[2]}),
         "west stack is accepted");
  const int llx = circuit.RegionLLX();
  const int urx = circuit.RegionURX();
  const int lly = circuit.RegionLLY();
  const int ury = circuit.RegionURY();
  Expect(a->IsFixed() && b->IsFixed() && c->IsFixed(), "west cells are FIXED");
  Expect(a->URX() == llx && b->URX() == llx && c->URX() == llx,
         "west cells are flush against the region, outside it");
  Expect(b->LLY() == a->URY() && c->LLY() == b->URY(),
         "west cells are contiguous in the order given");
  const double middle = (a->LLY() + c->URY()) / 2.0;
  Expect(middle >= (lly + ury) / 2.0 - 1 && middle <= (lly + ury) / 2.0 + 1,
         "west stack is centred on the edge");

  Expect(!dali.ExecuteCommand({"place-environment", "west", names[3]}),
         "a reused edge is rejected");
  Expect(!dali.ExecuteCommand({"place-environment", "east", names[0]}),
         "an already fixed component is rejected");
  Component *d = circuit.GetComponentPtr(names[3]);
  Expect(d->IsMovable(), "rejected request left its cell movable");

  Expect(dali.ExecuteCommand({"place-environment", "north", names[3],
                              names[4]}),
         "north stack is accepted");
  Component *e = circuit.GetComponentPtr(names[4]);
  Expect(d->LLY() == ury && e->LLY() == ury,
         "north cells sit on the region's top edge, outside it");
  Expect(e->LLX() == d->URX(), "north cells are contiguous along x");

  // More movable cells than fit along the south edge.
  std::vector<std::string> wide;
  long long width = 0;
  for (Component &component : circuit.Components()) {
    if (!component.IsMovable()) continue;
    wide.push_back(component.Name());
    width += component.Width();
    if (width > urx - llx && wide.size() > 3) break;
  }
  Expect(width > urx - llx, "found enough cells to overflow the south edge");
  std::vector<std::string> south_only{"place-environment", "south"};
  south_only.insert(south_only.end(), wide.begin(), wide.end());
  Expect(!dali.ExecuteCommand(south_only), "an overflowing stack is rejected");
  bool all_movable = true;
  for (const std::string &name : wide) {
    all_movable &= circuit.GetComponentPtr(name)->IsMovable();
  }
  Expect(all_movable, "a rejected stack leaves every cell movable");

  std::vector<std::string> spill{"place-environment", "south,east"};
  spill.insert(spill.end(), wide.begin(), wide.end());
  Expect(dali.ExecuteCommand(spill), "the stack spills onto the next edge");
  Component *first = circuit.GetComponentPtr(wide.front());
  Component *last = circuit.GetComponentPtr(wide.back());
  Expect(first->URY() == lly && first->IsFixed(),
         "the first cells sit outside the south edge");
  Expect(last->LLX() == urx && last->IsFixed(),
         "the remaining cells continue outside the east edge");
  for (std::size_t k = 1; k < wide.size(); ++k) {
    Component *a_cell = circuit.GetComponentPtr(wide[k - 1]);
    Component *b_cell = circuit.GetComponentPtr(wide[k]);
    if (b_cell->URY() == lly) {
      Expect(b_cell->LLX() == a_cell->URX(),
             "south cells are contiguous in the order given");
    }
  }

  dali.Close();
  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("place-environment: all checks passed\n");
  return 0;
}
