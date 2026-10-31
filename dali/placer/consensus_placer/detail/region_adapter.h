#pragma once
#include "dali/placer/well_legalizer/multi_region_placement_validator.h"
#include "dali/placer/well_legalizer/stripe.h"
#include "objective_agreement.h"
#include "solver.h"
#include <map>
#include <unordered_map>

namespace dali::consensus_internal::regions {
using namespace ::dali::consensus_internal;
using Report = dali::MultiRegionPlacementLegalityReport;
inline Report validate(dali::Circuit &c,
                       std::vector<dali::StripeColumn> &columns,
                       dali::GriddedPlacementValidationConfig config = {}) {
  return dali::ValidateMultiRegionPlacement(c, columns, config);
}
struct Result {
  ::dali::consensus_internal::Result solver;
  Report source, trial;
  bool committed = false;
  double preparation_seconds = 0, validation_seconds = 0, total_seconds = 0;
  double source_model_error_um = 0, source_model_bound_um = 0;
  double trial_model_error_um = 0, trial_model_bound_um = 0;
};
inline Result refine(dali::Circuit &c, std::vector<dali::StripeColumn> &columns,
                     const Options &options = {},
                     std::function<bool()> additional_check = {},
                     Site max_move_sites = -1, double displacement_weight = 0,
                     dali::GriddedPlacementValidationConfig config = {}) {
  using Clock = std::chrono::steady_clock;
  auto started = Clock::now();
  auto now = [&] {
    return std::chrono::duration<double>(Clock::now() - started).count();
  };
  Result result;
  double began = now();
  result.source = validate(c, columns, config);
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
  std::vector<dali::Component *> obstacles;
  for (auto &cell : c.Components())
    if (cell.IsFixed())
      obstacles.push_back(&cell);
  for (auto &cell : c.design().WellTaps())
    obstacles.push_back(&cell);
  for (auto &cell : c.design().EndCapComponentCollection().Instances())
    obstacles.push_back(&cell);
  for (std::size_t i = 0; i < owners.size(); ++i)
    for (auto *fixed : obstacles) {
      auto *cell = owners[i];
      if (cell->LLY() >= fixed->URY() || cell->URY() <= fixed->LLY())
        continue;
      if (cell->URX() <= fixed->LLX())
        m.cells[i].hi = std::min(
            m.cells[i].hi, Site(std::floor(fixed->LLX() - cell->Width())));
      else if (cell->LLX() >= fixed->URX())
        m.cells[i].lo = std::max(m.cells[i].lo, Site(std::ceil(fixed->URX())));
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
  const double native_source_hpwl = c.WeightedHPWLX();
  result.solver = ::dali::consensus_internal::refine(m, options);
  result.source_model_error_um =
      std::abs(result.solver.source_hpwl - native_source_hpwl);
  result.source_model_bound_um = hpwl_agreement_bound(
      native_source_hpwl, result.solver.source_hpwl, c.Nets().size());
  require(hpwl_agrees(native_source_hpwl, result.solver.source_hpwl,
                      c.Nets().size()),
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
    result.trial = validate(c, columns, config);
    bool extra = !additional_check || additional_check();
    result.validation_seconds += now() - began;
    double wire = c.WeightedHPWLX();
    result.trial_model_error_um = std::abs(wire - result.solver.hpwl);
    result.trial_model_bound_um =
        hpwl_agreement_bound(wire, result.solver.hpwl, c.Nets().size());
    if (result.trial.violations() == 0 && extra &&
        hpwl_agrees(wire, result.solver.hpwl, c.Nets().size()) &&
        wire + result.solver.displacement <
            native_source_hpwl - options.minimum_improvement) {
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
