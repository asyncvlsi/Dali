#include "multi_region_placement_validator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>
namespace dali {
inline double pn_edge(dali::Macro *macro, int r) {
  return macro->IsNwellAbovePwell(r) ? macro->PwellRect(r).URY()
                                     : macro->NwellRect(r).URY();
}
// Independent of old row legality helpers and of the reduced consensus model.
MultiRegionPlacementLegalityReport
ValidateMultiRegionPlacement(dali::Circuit &c,
                             std::vector<dali::StripeColumn> &columns,
                             GriddedPlacementValidationConfig config) {
  MultiRegionPlacementLegalityReport out;
  std::unordered_map<dali::Component *,
                     std::map<int, std::pair<dali::Stripe *, int>>>
      occurrences;
  std::set<dali::Component *> inventory;
  for (auto &cell : c.Components())
    if (cell.IsMovable() && cell.Area() > 0)
      inventory.insert(&cell);
  for (auto &col : columns)
    for (auto &stripe : col.stripe_list_) {
      int previous_top = INT32_MIN;
      for (std::size_t j = 0; j < stripe.gridded_rows_.size(); ++j) {
        auto &row = stripe.gridded_rows_[j];
        if (row.LLX() < stripe.LLX() || row.URX() > stripe.URX() ||
            row.LLY() < stripe.LLY() || row.URY() > stripe.URY() ||
            row.LLY() < previous_top || row.LLX() < c.RegionLLX() ||
            row.URX() > c.RegionURX() || row.LLY() < c.RegionLLY() ||
            row.URY() > c.RegionURY())
          ++out.geometry;
        previous_top = row.URY();
        std::vector<dali::Component *> row_cells;
        for (auto cr : row.ComponentRegions()) {
          auto *cell = cr.component;
          if (!inventory.count(cell)) {
            ++out.inventory;
            continue;
          }
          auto *macro = cell->MacroPtr();
          int count = macro->RegionCount();
          if (!macro->HasCompleteWellRegions() || cr.region_id < 0 ||
              cr.region_id >= count) {
            ++out.regions;
            continue;
          }
          if (!occurrences[cell]
                   .emplace(cr.region_id, std::make_pair(&stripe, int(j)))
                   .second)
            ++out.regions;
          row_cells.push_back(cell);
          bool flipped = cell->Orient() == dali::FS;
          if (cell->Orient() != dali::N && !flipped) {
            ++out.alignment;
            continue;
          }
          int canonical = flipped ? count - 1 - cr.region_id : cr.region_id;
          bool above = macro->IsNwellAbovePwell(canonical);
          if (flipped)
            above = !above;
          double edge = pn_edge(macro, canonical);
          int prefix = 0, total = 0;
          for (int k = 0; k < int(cell->StretchLengths().size()); ++k) {
            total += cell->StretchLengths()[k];
            if (k < canonical)
              prefix += cell->StretchLengths()[k];
          }
          double physical_edge =
              cell->LLY() + (flipped ? macro->Height() + total - edge - prefix
                                     : edge + prefix);
          if (above != row.IsOrientN() ||
              std::abs(physical_edge - row.LLY() - row.PNEdge()) > 1e-9 ||
              macro->PwellHeight(cr.region_id, flipped) > row.PHeight() ||
              macro->NwellHeight(cr.region_id, flipped) > row.NHeight())
            ++out.alignment;
          if (cell->LLX() < row.LLX() + row.LeftBoundaryMargin() ||
              cell->URX() > row.URX() - row.RightBoundaryMargin() ||
              !std::isfinite(cell->LLX()) ||
              cell->LLX() != std::round(cell->LLX()))
            ++out.geometry;
          if (!row.Segments().empty()) {
            bool found = false;
            for (auto &segment : row.Segments())
              found |=
                  cell->LLX() >= segment.LLX() && cell->URX() <= segment.URX();
            if (!found)
              ++out.geometry;
          }
        }
        std::sort(row_cells.begin(), row_cells.end(),
                  [](auto *a, auto *b) { return a->LLX() < b->LLX(); });
        for (std::size_t k = 1; k < row_cells.size(); ++k)
          if (row_cells[k]->LLX() < row_cells[k - 1]->URX())
            ++out.geometry;
      }
    }
  for (auto *cell : inventory) {
    auto *macro = cell->MacroPtr();
    int count = macro->RegionCount();
    auto found = occurrences.find(cell);
    if (found == occurrences.end() || int(found->second.size()) != count) {
      ++out.inventory;
      continue;
    }
    if (cell->StretchLengths().size() != std::size_t(count - 1)) {
      ++out.stretch;
      continue;
    }
    for (int s : cell->StretchLengths())
      if (s < 0)
        ++out.stretch;
    auto &occ = found->second;
    auto first = occ.find(0);
    if (first == occ.end()) {
      ++out.regions;
      continue;
    }
    auto *stripe = first->second.first;
    int start = first->second.second;
    if (cell->LLX() < stripe->LLX() || cell->URX() > stripe->URX() ||
        cell->LLY() < stripe->LLY() || cell->URY() > stripe->URY())
      ++out.geometry;
    for (int k = 0; k < count; ++k) {
      auto it = occ.find(k);
      if (it == occ.end() || it->second.first != stripe ||
          it->second.second != start + k) {
        ++out.regions;
        continue;
      }
      if (k) {
        auto &row = stripe->gridded_rows_[start + k];
        auto &prev = stripe->gridded_rows_[start + k - 1];
        int actual = row.LLY() + row.PNEdge() - prev.LLY() - prev.PNEdge();
        int expected = actual - macro->AdjacentRegionEdgeDistance(
                                    k - 1, cell->IsFlipped());
        int canonical = cell->IsFlipped() ? count - 1 - k : k - 1;
        if (cell->StretchLengths()[canonical] != expected)
          ++out.stretch;
      }
    }
  }
  // Full rectangles catch cross-row physical collisions, including fixed
  // objects omitted from row lists. This initial adapter uses an X sweep.
  std::vector<dali::Component *> all;
  for (auto &cell : c.Components())
    if (cell.Area() > 0)
      all.push_back(&cell);
  for (auto &cell : c.design().WellTaps())
    all.push_back(&cell);
  for (auto &cell : c.design().EndCapComponentCollection().Instances())
    all.push_back(&cell);
  std::sort(all.begin(), all.end(),
            [](auto *a, auto *b) { return a->LLX() < b->LLX(); });
  for (std::size_t i = 0; i < all.size(); ++i)
    for (std::size_t j = i + 1; j < all.size() && all[j]->LLX() < all[i]->URX();
         ++j)
      if ((all[i]->IsMovable() || all[j]->IsMovable()) &&
          all[i]->LLY() < all[j]->URY() && all[j]->LLY() < all[i]->URY())
        ++out.geometry;
  if (out.inventory)
    return out;
  config.use_component_regions = true;
  out.physical = GriddedPlacementValidator(&c, &columns, config).Validate();
  return out;
}
} // namespace dali
