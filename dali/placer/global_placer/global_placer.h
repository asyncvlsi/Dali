/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_

#include <functional>
#include <string>
#include <vector>

#include "dali/placer/global_placer/hpwl_optimizer.h"
#include "dali/placer/global_placer/random_initializer.h"
#include "dali/placer/global_placer/rough_legalizer.h"
#include "dali/placer/placer.h"

namespace dali {

/** Global placement flow combining initialization, HPWL optimization, and rough
 * legalization. */
class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  /** Set maximum global placement iterations. */
  void SetMaxIteration(int max_iter);

  /** Enable or disable intermediate placement dumps. */
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);

  /** Callback used by the application to emit visualization snapshots. */
  using SnapshotCallback =
      std::function<void(const std::string& id, const std::string& label,
                         const std::string& subgroup, int iteration)>;

  /** Set a callback invoked after each global-placement iteration step. */
  void SetSnapshotCallback(SnapshotCallback snapshot_callback);

  /** Select how movable component locations are initialized before placement.
   */
  void SetInitializerType(RandomInitializerType initializer_type);

  /** Select how anchor pseudo-net strength changes across iterations. */
  void SetAnchorSchedule(GlobalAnchorSchedule schedule);

  /** Select how look-ahead legalization grid dimensions are refined. */
  void SetGridSchedule(GlobalGridSchedule schedule);

  /** Select how LAL grows overfilled clusters into whitespace regions. */
  void SetLalExpansionMode(GlobalLalExpansionMode mode);

  /** Select whether fixed-macro boundaries influence LAL cutlines. */
  void SetLalMacroBoundaryMode(GlobalLalMacroBoundaryMode mode);

  /** Load global placer configuration. */
  void LoadConf(std::string const& config_file) override;

  /** Create optimizer and rough legalizer instances. */
  void InitializeOptimizerAndLegalizer();

  /** Release optimizer and rough legalizer instances. */
  void CloseOptimizerAndLegalizer();

  /** Initialize component locations before iterative placement. */
  void InitializeComponentLocation();

  /** Run global placement. */
  bool StartPlacement() override;

 protected:
  // Iteration and convergence controls for look-ahead legalization.
  int cur_iter_ = 0;
  int max_iter_ = 100;
  double simpl_LAL_converge_criterion_ = 0.005;
  double polar_converge_criterion_ = 0.08;
  int convergence_criteria_ = 1;

  // Save intermediate result for debugging and/or visualization.
  bool should_save_intermediate_result_ = false;

  bool IsComponentListOrNetListEmpty() const;
  static bool IsSeriesConverged(std::vector<double>& series, int window_size,
                                double tolerance);
  bool IsPlacementConverged();
  void PreparePlacement();
  void RunPlacementIterations();
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& subgroup, int iteration);
  void EmitIterationSnapshot(const std::string& id_suffix,
                             const std::string& label_suffix,
                             const std::string& subgroup);
  void FinalizePlacement();
  void PrintHpwl() const;
  void PrintEndStatement(std::string const& name_of_process,
                         bool is_success) override;

  RandomInitializerType initializer_type_ = RandomInitializerType::UNIFORM;
  GlobalAnchorSchedule anchor_schedule_ = GlobalAnchorSchedule::kDali;
  GlobalGridSchedule grid_schedule_ = GlobalGridSchedule::kDali;
  GlobalLalExpansionMode lal_expansion_mode_ =
      GlobalLalExpansionMode::kSymmetric;
  GlobalLalMacroBoundaryMode lal_macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
  SnapshotCallback snapshot_callback_;
  HpwlOptimizer* optimizer_ = nullptr;
  RoughLegalizer* legalizer_ = nullptr;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
