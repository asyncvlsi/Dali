#pragma once
#include "dali/placer/well_legalizer/gridded_placement_validator.h"
#include "solver.h"
#include <unordered_map>

namespace dali::consensus_internal {
struct DaliOptions {
  Options solver;
  // Cost is component-pin weighted horizontal HPWL in microns plus this
  // coefficient times L1 horizontal displacement from this invocation's input.
  double displacement_weight = 0;
  // A nonnegative cap restricts |x-input_x|; -1 leaves the connected interval.
  Site max_move_sites = -1;
  dali::GriddedPlacementValidationConfig validation;
  // Optional design-specific fence/spacing check. Called on the source and
  // trial placement. Exceptions on a trial also trigger coordinate rollback.
  std::function<bool()> additional_placement_check;
};
struct DaliResult {
  Result proposal;
  bool committed = false;
  std::string status;
  std::size_t initial_violations = 0, trial_violations = 0;
  double hpwl_before_um = 0, hpwl_after_um = 0;
  double validation_seconds = 0, total_seconds = 0;
};

// Opt-in call AFTER row assignment/X legalization. Columns and circuit must
// refer to the same design. No orientation, Y, assignment, or row order edits.
inline DaliResult refine_gridded(dali::Circuit &circuit,
                                 std::vector<dali::StripeColumn> &columns,
                                 const DaliOptions &options = {}) {
  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  auto elapsed = [&] {
    return std::chrono::duration<double>(Clock::now() - start).count();
  };
  require(finite(options.displacement_weight) &&
              options.displacement_weight >= 0,
          "invalid Dali displacement weight");
  require(options.max_move_sites >= -1 &&
              options.max_move_sites <= (Site(1) << 40),
          "invalid move cap");
  DaliResult result;
  dali::GriddedPlacementValidator validator(&circuit, &columns,
                                            options.validation);
  auto before_validate = elapsed();
  auto initial = validator.Validate();
  bool source_additional_ok = !options.additional_placement_check ||
                              options.additional_placement_check();
  result.validation_seconds += elapsed() - before_validate;
  result.initial_violations = initial.TotalViolationCount();
  if (!initial.IsLegal() || !source_additional_ok) {
    result.status = "invalid_source";
    result.total_seconds = elapsed();
    return result;
  }
  double pitch = circuit.GridValueX();
  require(finite(pitch) && pitch > 0, "invalid Dali X grid");
  std::vector<dali::Component *> owners;
  std::unordered_map<dali::Component *, int> index;
  Model model;
  auto site = [](double x) -> Site {
    require(finite(x) && std::abs(x) <= double(Site(1) << 40) &&
                x == std::round(x),
            "gridded source must use integral X sites");
    return Site(std::round(x));
  };
  // Keep the original side of every fixed rectangle intersecting this cell's
  // vertical span. Tap/end-cap margins and interior taps are also obstacles.
  std::vector<dali::Component *> obstacles;
  for (auto &c : circuit.Components())
    if (!c.IsMovable())
      obstacles.push_back(&c);
  for (auto &column : columns)
    for (auto &stripe : column.stripe_list_)
      for (auto &row : stripe.gridded_rows_) {
        for (auto *tap : row.TapCells())
          if (tap)
            obstacles.push_back(tap);
        Row constraint;
        std::vector<dali::Component *> sorted = row.Components();
        std::sort(sorted.begin(), sorted.end(),
                  [](auto *a, auto *b) { return a->LLX() < b->LLX(); });
        for (auto *c : sorted) {
          require(c && c->IsMovable() && index.count(c) == 0,
                  "invalid gridded row ownership");
          int i = int(owners.size());
          index[c] = i;
          owners.push_back(c);
          constraint.owners.push_back(i);
          Site x = site(c->LLX());
          Site lo = row.LLX() + row.LeftBoundaryMargin();
          Site hi = row.URX() - row.RightBoundaryMargin() - c->Width();
          if (options.max_move_sites >= 0) {
            lo = std::max(lo, x - options.max_move_sites);
            hi = std::min(hi, x + options.max_move_sites);
          }
          for (auto *fixed : obstacles) {
            if (fixed == c || c->LLY() >= fixed->URY() - 1e-9 ||
                c->URY() <= fixed->LLY() + 1e-9)
              continue;
            if (c->URX() <= fixed->LLX() + 1e-9)
              hi = std::min(hi,
                            Site(std::floor(fixed->LLX() - c->Width() + 1e-9)));
            else if (c->LLX() >= fixed->URX() - 1e-9)
              lo = std::max(lo, Site(std::ceil(fixed->URX() - 1e-9)));
            else
              require(false, "source overlaps fixed obstacle/tap");
          }
          model.cells.push_back(
              {x, lo, hi, double(x), options.displacement_weight * pitch});
          if (constraint.owners.size() > 1)
            constraint.separation.push_back(
                sorted[constraint.owners.size() - 2]->Width());
        }
        model.rows.push_back(std::move(constraint));
      }
  // Match Circuit::WeightedHPWLX exactly: ComponentPins includes Dali's dummy
  // IO components; separate Net::IoPinPtrs entries are not added by that
  // metric.
  for (auto &net : circuit.Nets()) {
    Net factor;
    factor.weight = net.Weight() * pitch;
    if (net.ComponentPins().size() <= 1)
      continue;
    for (auto &pin : net.ComponentPins()) {
      auto it = index.find(pin.ComponentPtr());
      if (it == index.end())
        factor.fixed_pins.push_back(pin.AbsX());
      else
        factor.pins.push_back({it->second, pin.OffsetX()});
    }
    model.nets.push_back(std::move(factor));
  }
  result.hpwl_before_um = circuit.WeightedHPWLX();
  result.hpwl_after_um = result.hpwl_before_um;
  result.proposal = refine(model, options.solver);
  require(std::abs(result.proposal.source_hpwl - result.hpwl_before_um) <=
              1e-8 * std::max(1., result.hpwl_before_um),
          "Dali/model HPWL mismatch");
  if (result.proposal.owners_changed == 0) {
    result.status = "retained_source";
    result.total_seconds = elapsed();
    return result;
  }
  struct Rollback {
    std::vector<dali::Component *> &owners;
    const Model &model;
    bool keep = false;
    ~Rollback() {
      if (!keep)
        for (std::size_t i = 0; i < owners.size(); ++i)
          owners[i]->SetLLX(double(model.cells[i].source));
    }
  } rollback{owners, model};
  for (std::size_t i = 0; i < owners.size(); ++i)
    owners[i]->SetLLX(double(result.proposal.x[i]));
  before_validate = elapsed();
  auto trial = validator.Validate();
  result.trial_violations = trial.TotalViolationCount();
  bool additional_ok = !options.additional_placement_check ||
                       options.additional_placement_check();
  result.validation_seconds += elapsed() - before_validate;
  double native_hpwl = circuit.WeightedHPWLX();
  bool agreement = std::abs(native_hpwl - result.proposal.hpwl) <=
                   1e-8 * std::max(1., result.hpwl_before_um);
  double native_cost = native_hpwl + result.proposal.displacement;
  if (trial.IsLegal() && additional_ok && agreement &&
      native_cost < result.proposal.source_objective -
                        options.solver.minimum_improvement) {
    rollback.keep = true;
    result.committed = true;
    result.hpwl_after_um = native_hpwl;
    result.status = "committed";
  } else
    result.status = !trial.IsLegal() ? "rejected_physical_validation"
                    : !additional_ok ? "rejected_additional_validation"
                                     : "rejected_objective_check";
  if (!rollback.keep) {
    for (std::size_t i = 0; i < owners.size(); ++i)
      owners[i]->SetLLX(double(model.cells[i].source));
    rollback.keep =
        true; // already restored; include this work in total_seconds
  }
  result.total_seconds = elapsed();
  return result;
}
} // namespace dali::consensus_internal
