/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 * SPDX-License-Identifier: GPL-2.0-or-later
 ******************************************************************************/
#ifndef DALI_PLACER_CONSENSUS_PLACER_CONSENSUS_PLACER_H_
#define DALI_PLACER_CONSENSUS_PLACER_CONSENSUS_PLACER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "dali/placer/placer.h"
#include "dali/placer/well_legalizer/gridded_placement_validator.h"

namespace dali {

/** Explicit ownership model for an already legalized placement. */
enum class ConsensusRowModel { kGridded, kMultiRegion };

struct ConsensusPlacerOptions {
  int max_iterations = 128;
  int recover_every = 8;
  int stagnant_checkpoints = 4;
  double time_limit_seconds = 1.0;
  double rho = 1.0;
  double primal_tolerance = 1e-5;
  double dual_tolerance = 1e-5;
  double minimum_improvement = 1e-9;
  // Horizontal L1 movement from this invocation's input, added to HPWL.
  double displacement_weight = 0.0;
  // -1 means the connected source interval; nonnegative values cap X movement.
  std::int64_t max_move_sites = -1;
  GriddedPlacementValidationConfig validation;
  // Called only at safe boundaries; true retains the last legal incumbent.
  std::function<bool()> cancelled;
};

struct ConsensusPlacerReport {
  bool success = false;
  bool committed = false;
  bool source_legal = false;
  std::string stop_reason;
  std::string error;
  std::size_t owners_considered = 0;
  std::size_t owners_updated = 0;
  std::size_t owners_changed = 0;
  std::size_t row_copies = 0;
  std::size_t pin_copies = 0;
  int iterations = 0;
  int recoveries = 0;
  double source_hpwl_x_um = 0;
  double hpwl_x_um = 0;
  double source_hpwl_y_um = 0;
  double hpwl_y_um = 0;
  double objective = 0;
  double weighted_displacement = 0;
  double primal_residual = 0;
  double dual_residual = 0;
  double preparation_seconds = 0;
  double optimizer_seconds = 0;
  double validation_seconds = 0;
  double total_seconds = 0;
};

/**
 * Optional horizontal refinement of a legal gridded/mixed-height placement.
 *
 * One physical owner is shared by every occupied row and incident net. Row
 * assignment, X order, Y, orientation and stretching remain fixed. Only a
 * complete, legal, strictly improving integer recovery is committed. Input
 * coordinates are restored on rejection or error. The time limit is
 * cooperative and excludes application setup and physical validation.
 *
 * This placer neither initializes row assignments nor replaces a legalizer.
 * It does not run unless explicitly called or enabled by the application.
 */
class ConsensusPlacer : public Placer {
public:
  void SetRows(std::vector<StripeColumn> *columns, ConsensusRowModel model);
  void SetOptions(const ConsensusPlacerOptions &options);
  /** Optional read-only fence/technology policy, checked on source and trial.
   */
  void SetAdditionalPlacementCheck(std::function<bool()> check);
  bool StartPlacement() override;
  const ConsensusPlacerReport &Report() const { return report_; }

private:
  std::vector<StripeColumn> *columns_ = nullptr;
  ConsensusRowModel row_model_ = ConsensusRowModel::kGridded;
  ConsensusPlacerOptions options_;
  std::function<bool()> additional_check_;
  ConsensusPlacerReport report_;
};

} // namespace dali
#endif
