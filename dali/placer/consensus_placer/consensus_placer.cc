/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 * SPDX-License-Identifier: GPL-2.0-or-later
 ******************************************************************************/
#include "consensus_placer.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "detail/gridded_adapter.h"
#include "detail/region_adapter.h"

namespace dali {
void ConsensusPlacer::SetRows(std::vector<StripeColumn> *columns,
                              ConsensusRowModel model) {
  columns_ = columns;
  row_model_ = model;
}

void ConsensusPlacer::SetOptions(const ConsensusPlacerOptions &options) {
  options_ = options;
}

void ConsensusPlacer::SetAdditionalPlacementCheck(std::function<bool()> check) {
  additional_check_ = std::move(check);
}

bool ConsensusPlacer::StartPlacement() {
  using Clock = std::chrono::steady_clock;
  const auto started = Clock::now();
  auto elapsed = [&] {
    return std::chrono::duration<double>(Clock::now() - started).count();
  };
  report_ = {};
  struct Pose {
    Component *cell;
    double x, y;
    ComponentOrient orient;
  };
  std::vector<Pose> input;
  auto restore = [&] {
    for (const auto &pose : input) {
      pose.cell->SetLLX(pose.x);
      pose.cell->SetLLY(pose.y);
      pose.cell->SetOrient(pose.orient);
    }
  };
  try {
    using consensus_internal::require;
    require(ckt_ptr_ != nullptr && columns_ != nullptr,
            "ConsensusPlacer needs a Circuit and legalized rows");
    require(row_model_ == ConsensusRowModel::kGridded ||
                row_model_ == ConsensusRowModel::kMultiRegion,
            "Unknown consensus row model");
    require(std::isfinite(options_.displacement_weight) &&
                options_.displacement_weight >= 0 &&
                options_.max_move_sites >= -1 &&
                options_.max_move_sites <= (std::int64_t(1) << 40),
            "Invalid consensus movement controls");

    std::unordered_set<Component *> known;
    for (auto &cell : ckt_ptr_->Components()) {
      require(std::isfinite(cell.LLX()) && std::isfinite(cell.LLY()),
              "Nonfinite input placement");
      input.push_back({&cell, cell.LLX(), cell.LLY(), cell.Orient()});
      known.insert(&cell);
    }
    // The existing row validators may sort/dereference entries before reporting
    // invalid references, so reject foreign/null entries at this boundary.
    for (auto &column : *columns_) {
      for (auto &stripe : column.stripe_list_) {
        for (auto &row : stripe.gridded_rows_) {
          auto check = [&](Component *cell) {
            require(known.count(cell) && cell->IsMovable() && cell->Area() > 0,
                    "Invalid consensus row owner");
            require(cell->MacroPtr() &&
                        cell->MacroPtr()->HasCompleteWellRegions(),
                    "Consensus needs complete N/P well regions");
            if (row_model_ == ConsensusRowModel::kGridded) {
              require(cell->MacroPtr()->RegionCount() == 1,
                      "Multi-region cells need the multi-region row model");
            }
          };
          if (row_model_ == ConsensusRowModel::kGridded) {
            for (auto *cell : row.Components())
              check(cell);
          } else {
            for (auto region : row.ComponentRegions())
              check(region.component);
          }
        }
      }
    }

    consensus_internal::Options solver;
    solver.max_iterations = options_.max_iterations;
    solver.recover_every = options_.recover_every;
    solver.stagnant_checkpoints = options_.stagnant_checkpoints;
    solver.time_limit_seconds = options_.time_limit_seconds;
    solver.rho = options_.rho;
    solver.primal_tolerance = options_.primal_tolerance;
    solver.dual_tolerance = options_.dual_tolerance;
    solver.minimum_improvement = options_.minimum_improvement;
    solver.cancelled = options_.cancelled;
    report_.source_hpwl_x_um = ckt_ptr_->WeightedHPWLX();
    report_.source_hpwl_y_um = ckt_ptr_->WeightedHPWLY();
    consensus_internal::Result result;
    double adapter_preparation = 0;
    if (row_model_ == ConsensusRowModel::kGridded) {
      consensus_internal::DaliOptions adapter;
      adapter.solver = solver;
      adapter.displacement_weight = options_.displacement_weight;
      adapter.max_move_sites = options_.max_move_sites;
      adapter.validation = options_.validation;
      adapter.additional_placement_check = additional_check_;
      auto refined =
          consensus_internal::refine_gridded(*ckt_ptr_, *columns_, adapter);
      report_.source_legal =
          refined.initial_violations == 0 && refined.status != "invalid_source";
      require(report_.source_legal, "Invalid gridded consensus source");
      result = std::move(refined.proposal);
      report_.committed = refined.committed;
      report_.validation_seconds = refined.validation_seconds;
      adapter_preparation =
          refined.total_seconds - result.seconds - refined.validation_seconds;
      report_.stop_reason = refined.status.rfind("rejected_", 0) == 0
                                ? refined.status
                                : result.stop;
    } else {
      auto refined = consensus_internal::regions::refine(
          *ckt_ptr_, *columns_, solver, additional_check_,
          options_.max_move_sites, options_.displacement_weight,
          options_.validation);
      report_.source_legal = refined.source.violations() == 0;
      result = std::move(refined.solver);
      report_.committed = refined.committed;
      report_.validation_seconds = refined.validation_seconds;
      adapter_preparation = refined.preparation_seconds;
      report_.stop_reason = result.owners_changed && !refined.committed
                                ? "rejected_trial"
                                : result.stop;
    }

    report_.hpwl_x_um = ckt_ptr_->WeightedHPWLX();
    report_.hpwl_y_um = ckt_ptr_->WeightedHPWLY();
    require(std::isfinite(report_.hpwl_x_um) &&
                report_.hpwl_y_um == report_.source_hpwl_y_um,
            "Consensus changed vertical pin geometry");
    report_.weighted_displacement = report_.committed ? result.displacement : 0;
    report_.objective = report_.hpwl_x_um + report_.weighted_displacement;
    require(report_.objective <= report_.source_hpwl_x_um + 1e-9,
            "Consensus retained a worse objective");
    for (const auto &pose : input) {
      require(pose.cell->LLY() == pose.y &&
                  pose.cell->Orient() == pose.orient &&
                  (pose.cell->IsMovable() || pose.cell->LLX() == pose.x),
              "Consensus changed a fixed pose");
      report_.owners_changed += pose.cell->IsMovable() &&
                                pose.cell->Area() > 0 &&
                                pose.cell->LLX() != pose.x;
    }
    report_.owners_considered = result.owners_considered;
    report_.owners_updated = result.iterations ? result.owners_considered : 0;
    report_.row_copies = result.row_copies;
    report_.pin_copies = result.pin_copies;
    report_.iterations = result.iterations;
    report_.recoveries = result.recoveries;
    report_.optimizer_seconds = result.seconds;
    report_.primal_residual = result.primal_residual;
    report_.dual_residual = result.dual_residual;
    report_.preparation_seconds = std::max(0., adapter_preparation);
    report_.success = true;
  } catch (const std::exception &error) {
    restore();
    report_.success = false;
    report_.committed = false;
    report_.owners_changed = 0;
    report_.weighted_displacement = 0;
    report_.hpwl_x_um = report_.source_hpwl_x_um;
    report_.hpwl_y_um = report_.source_hpwl_y_um;
    report_.objective = report_.source_hpwl_x_um;
    report_.stop_reason = "error";
    report_.error = error.what();
  }
  report_.total_seconds = elapsed();
  return report_.success;
}
} // namespace dali
