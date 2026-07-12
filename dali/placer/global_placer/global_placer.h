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
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "dali/placer/global_placer/hpwl_optimizer.h"
#include "dali/placer/global_placer/global_spreader.h"
#include "dali/placer/global_placer/global_upper_bound_refiner.h"
#include "dali/placer/global_placer/look_ahead_spreader.h"
#include "dali/placer/global_placer/placement_initializer.h"
#include "dali/placer/placer.h"

namespace dali {

/** Global placement flow combining initialization, HPWL optimization, and rough
 * legalization. */
class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  /** Set maximum global placement iterations. */
  void SetMaxIteration(int max_iter);

  /** Set minimum global placement iterations before convergence can stop. */
  void SetMinIteration(int min_iter);

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
  void SetInitializerType(PlacementInitializerType initializer_type);

  /** Select how anchor pseudo-net strength changes across iterations. */
  void SetAnchorSchedule(GlobalAnchorSchedule schedule);

  /** Select how look-ahead legalization grid dimensions are refined. */
  void SetGridSchedule(GlobalGridSchedule schedule);

  /** Select how LAL grows overfilled clusters into whitespace regions. */
  void SetLalExpansionMode(GlobalLalExpansionMode mode);

  /** Select how LAL chooses the next overfilled cluster to spread. */
  void SetLalHotspotMode(GlobalLalHotspotMode mode);

  /** Set the affine geometry-preservation weight used by LAL leaf spreading. */
  void SetLalAffineScalingWeight(double weight);

  /** Select whether fixed-macro boundaries influence LAL cutlines. */
  void SetLalMacroBoundaryMode(GlobalLalMacroBoundaryMode mode);

  /** Set the regional capacity policy used by the global spreader. */
  void SetCapacityModel(
      std::shared_ptr<const PlacementCapacityModel> capacity_model);

  /** Install an optional periodic physical upper-bound refiner. */
  void SetUpperBoundRefiner(
      std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner,
      int warmup_iteration, int interval);

  /** Select whether a refined physical upper bound becomes the next anchor. */
  void SetUseRefinedUpperBoundAsAnchor(bool enable) {
    use_refined_upper_bound_as_anchor_ = enable;
  }

  /** Enable bounded feedback from persistent LAL-to-legal corrections. */
  void SetEnablePersistentLegalizationFeedback(bool enable) {
    enable_persistent_legalization_feedback_ = enable;
  }

  /** Load global placer configuration. */
  void LoadConf(std::string const& config_file) override;

  /** Create the lower-bound optimizer and global spreader. */
  void InitializePlacementEngines();

  /** Release the lower-bound optimizer and global spreader. */
  void ClosePlacementEngines();

  /** Initialize component locations before iterative placement. */
  void InitializeComponentLocation();

  /** Run global placement. */
  bool StartPlacement() override;

 protected:
  struct ComponentLocation;

  // Iteration and convergence controls for look-ahead legalization.
  int cur_iter_ = 0;
  int max_iter_ = 100;
  int min_iter_ = 10;
  double polar_converge_criterion_ = 0.08;
  int convergence_criteria_ = 1;
  // Stop only after best legalized HPWL has not improved by at least 0.2%
  // for several iterations while the lower/upper gap is already small.
  double upper_bound_min_improvement_ = 0.002;
  int upper_bound_improvement_patience_ = 5;

  // Save intermediate result for debugging and/or visualization.
  bool should_save_intermediate_result_ = false;

  bool IsComponentListOrNetListEmpty() const;
  /** Return the relative improvement from old_value to new_value. */
  static double RelativeImprovement(double old_value, double new_value);

  /** Treat tiny floating-point values as zero for HPWL ratio checks. */
  static bool IsPositive(double value);

  /** Return true when legalized HPWL has not meaningfully improved recently. */
  bool HasUpperBoundHpwlStalled(
      const std::vector<double>& upper_bound_hpwl) const;

  bool IsPlacementConverged();
  void PreparePlacement();
  void RunPlacementIterations();
  bool ShouldRefineUpperBound() const;
  /** Save component coordinates when the accepted upper bound improves. */
  void UpdateBestUpperBoundPlacement(double upper_bound_hpwl);
  /** Return a copy of all current component coordinates. */
  std::vector<ComponentLocation> SaveCurrentPlacement() const;
  /** Restore component coordinates from a complete placement copy. */
  void RestorePlacement(const std::vector<ComponentLocation>& placement);
  /** Log displacement introduced by physical upper-bound refinement. */
  void LogRefinementDisplacement(
      const std::vector<ComponentLocation>& placement_before_refinement);
  /** Update persistent correction state and the next explicit anchor target. */
  void UpdatePersistentLegalizationFeedback(
      const std::vector<ComponentLocation>& placement_before_refinement);
  /** Restore the lowest-HPWL accepted upper-bound placement. */
  void RestoreBestUpperBoundPlacement();
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& subgroup, int iteration);
  void EmitIterationSnapshot(const std::string& id_suffix,
                             const std::string& label_suffix,
                             const std::string& subgroup);
  void FinalizePlacement();
  void PrintHpwl() const;
  void PrintEndStatement(std::string const& name_of_process,
                         bool is_success) override;

  PlacementInitializerType initializer_type_ =
      PlacementInitializerType::kUniform;
  GlobalAnchorSchedule anchor_schedule_ = GlobalAnchorSchedule::kDali;
  GlobalGridSchedule grid_schedule_ = GlobalGridSchedule::kDali;
  GlobalLalExpansionMode lal_expansion_mode_ =
      GlobalLalExpansionMode::kSymmetric;
  GlobalLalHotspotMode lal_hotspot_mode_ = GlobalLalHotspotMode::kComponentArea;
  double lal_affine_scaling_weight_ = 0.65;
  GlobalLalMacroBoundaryMode lal_macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
  SnapshotCallback snapshot_callback_;
  std::shared_ptr<const PlacementCapacityModel> capacity_model_ =
      std::make_shared<AreaCapacityModel>();
  std::unique_ptr<HpwlOptimizer> optimizer_;
  std::unique_ptr<GlobalSpreader> spreader_;
  std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner_;
  std::vector<double> accepted_upper_bound_hpwl_;
  struct ComponentLocation {
    double lx = 0.0;
    double ly = 0.0;
  };
  std::vector<ComponentLocation> best_upper_bound_placement_;
  double best_upper_bound_hpwl_ = std::numeric_limits<double>::max();
  int upper_bound_refiner_warmup_ = 0;
  int upper_bound_refiner_interval_ = 1;
  bool use_refined_upper_bound_as_anchor_ = true;
  bool enable_persistent_legalization_feedback_ = false;
  std::vector<double> average_legalization_correction_x_;
  std::vector<double> average_legalization_correction_y_;
  std::vector<int> legalization_correction_streak_x_;
  std::vector<int> legalization_correction_streak_y_;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
