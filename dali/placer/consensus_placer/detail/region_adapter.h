#pragma once
#include "dali/placer/well_legalizer/stripe.h"
#include "solver.h"
#include <map>
#include <unordered_map>

namespace dali::consensus_internal::regions {
using namespace ::dali::consensus_internal;
struct Report {
  std::size_t inventory = 0, regions = 0, geometry = 0, alignment = 0,
              stretch = 0;
  std::size_t violations() const {
    return inventory + regions + geometry + alignment + stretch;
  }
};
inline double pn_edge(dali::Macro *macro, int r) {
  return macro->IsNwellAbovePwell(r) ? macro->PwellRect(r).URY()
                                     : macro->NwellRect(r).URY();
}
// Independent of old row legality helpers and of the reduced consensus model.
inline Report validate(dali::Circuit &c,
                       std::vector<dali::StripeColumn> &columns) {
  Report out;
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
              !finite(cell->LLX()) || cell->LLX() != std::round(cell->LLX()))
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
  std::sort(all.begin(), all.end(),
            [](auto *a, auto *b) { return a->LLX() < b->LLX(); });
  for (std::size_t i = 0; i < all.size(); ++i)
    for (std::size_t j = i + 1; j < all.size() && all[j]->LLX() < all[i]->URX();
         ++j)
      if ((all[i]->IsMovable() || all[j]->IsMovable()) &&
          all[i]->LLY() < all[j]->URY() && all[j]->LLY() < all[i]->URY())
        ++out.geometry;
  return out;
}
struct Result {
  ::dali::consensus_internal::Result solver;
  Report source, trial;
  bool committed = false;
  double preparation_seconds = 0, validation_seconds = 0, total_seconds = 0;
};
inline Result refine(dali::Circuit &c, std::vector<dali::StripeColumn> &columns,
                     const Options &options = {},
                     std::function<bool()> additional_check = {},
                     Site max_move_sites = -1, double displacement_weight = 0) {
  using Clock = std::chrono::steady_clock;
  auto started = Clock::now();
  auto now = [&] {
    return std::chrono::duration<double>(Clock::now() - started).count();
  };
  Result result;
  double began = now();
  result.source = validate(c, columns);
  result.validation_seconds = now() - began;
  require(result.source.violations() == 0, "invalid multi-region source");
  if (additional_check)
    require(additional_check(), "additional source check failed");
  std::vector<dali::Component *> owners;
  std::unordered_map<dali::Component *, int> ids;
  Model m;
  for (auto &cell : c.Components())
    if (cell.IsMovable() && cell.Area() > 0) {
      ids[&cell] = int(owners.size());
      owners.push_back(&cell);
      Site x = Site(cell.LLX());
      Site lo = c.RegionLLX(), hi = c.RegionURX() - cell.Width();
      if (max_move_sites >= 0) {
        lo = std::max(lo, x - max_move_sites);
        hi = std::min(hi, x + max_move_sites);
      }
      m.cells.push_back(
          {x, lo, hi, double(x), displacement_weight * c.GridValueX()});
    }
  for (auto &col : columns)
    for (auto &stripe : col.stripe_list_)
      for (auto &row : stripe.gridded_rows_) {
        Row constraint;
        std::vector<dali::Component *> members;
        for (auto cr : row.ComponentRegions())
          members.push_back(cr.component);
        std::sort(members.begin(), members.end(),
                  [](auto *a, auto *b) { return a->LLX() < b->LLX(); });
        for (std::size_t k = 0; k < members.size(); ++k) {
          auto *cell = members[k];
          int i = ids.at(cell);
          constraint.owners.push_back(i);
          auto &owner = m.cells[i];
          owner.lo =
              std::max(owner.lo, Site(row.LLX() + row.LeftBoundaryMargin()));
          owner.hi =
              std::min(owner.hi, Site(row.URX() - row.RightBoundaryMargin() -
                                      cell->Width()));
          if (!row.Segments().empty())
            for (auto &segment : row.Segments())
              if (cell->LLX() >= segment.LLX() &&
                  cell->URX() <= segment.URX()) {
                owner.lo = std::max(owner.lo, Site(segment.LLX()));
                owner.hi =
                    std::min(owner.hi, Site(segment.URX() - cell->Width()));
                break;
              }
          if (k)
            constraint.separation.push_back(members[k - 1]->Width());
        }
        m.rows.push_back(std::move(constraint));
      }
  for (std::size_t i = 0; i < owners.size(); ++i)
    for (auto &fixed : c.Components())
      if (fixed.IsFixed() && fixed.Area() > 0) {
        auto *cell = owners[i];
        if (cell->LLY() >= fixed.URY() || cell->URY() <= fixed.LLY())
          continue;
        if (cell->URX() <= fixed.LLX())
          m.cells[i].hi = std::min(
              m.cells[i].hi, Site(std::floor(fixed.LLX() - cell->Width())));
        else if (cell->LLX() >= fixed.URX())
          m.cells[i].lo = std::max(m.cells[i].lo, Site(std::ceil(fixed.URX())));
      }
  for (auto &net : c.Nets()) {
    Net f;
    f.weight = net.Weight() * c.GridValueX();
    if (net.ComponentPins().size() < 2)
      continue;
    for (auto p : net.ComponentPins()) {
      auto it = ids.find(p.ComponentPtr());
      if (it == ids.end())
        f.fixed_pins.push_back(p.AbsX());
      else
        f.pins.push_back({it->second, p.OffsetX()});
    }
    m.nets.push_back(std::move(f));
  }
  result.preparation_seconds = now() - result.validation_seconds;
  result.solver = ::dali::consensus_internal::refine(m, options);
  require(std::abs(result.solver.source_hpwl - c.WeightedHPWLX()) < 1e-7,
          "region HPWL model mismatch");
  result.trial = result.source;
  if (result.solver.owners_changed) {
    struct Rollback {
      std::vector<dali::Component *> &owners;
      const Model &model;
      bool keep = false;
      ~Rollback() {
        if (!keep)
          for (std::size_t i = 0; i < owners.size(); ++i)
            owners[i]->SetLLX(double(model.cells[i].source));
      }
    } rollback{owners, m};
    for (std::size_t i = 0; i < owners.size(); ++i)
      owners[i]->SetLLX(double(result.solver.x[i]));
    began = now();
    result.trial = validate(c, columns);
    bool extra = !additional_check || additional_check();
    result.validation_seconds += now() - began;
    double wire = c.WeightedHPWLX();
    if (result.trial.violations() == 0 && extra &&
        std::abs(wire - result.solver.hpwl) < 1e-7 &&
        wire + result.solver.displacement <
            result.solver.source_objective - options.minimum_improvement) {
      rollback.keep = true;
      result.committed = true;
    } else {
      for (std::size_t i = 0; i < owners.size(); ++i)
        owners[i]->SetLLX(double(m.cells[i].source));
      rollback.keep = true;
    }
  }
  result.total_seconds = now();
  return result;
}
} // namespace dali::consensus_internal::regions
