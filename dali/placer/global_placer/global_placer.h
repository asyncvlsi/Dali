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

#include "dali/placer/global_placer/global_spreader.h"
#include "dali/placer/global_placer/global_upper_bound_refiner.h"
#include "dali/placer/global_placer/hpwl_optimizer.h"
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

  /** Set the pin-count cutoff for nets omitted from the quadratic model. */
  void SetNetIgnoreThreshold(int net_ignore_threshold);

  /** Select how look-ahead legalization grid dimensions are refined. */

  /** Select how LAL grows overfilled clusters into whitespace regions. */
  void SetLalExpansionMode(GlobalLalExpansionMode mode);

  /** Select how LAL chooses the next overfilled cluster to spread. */
  void SetLalHotspotMode(GlobalLalHotspotMode mode);

  /** Set the affine geometry-preservation weight used by LAL leaf spreading. */
  void SetLalAffineScalingWeight(double weight);

  /** Select whether fixed-macro boundaries influence LAL cutlines. */
  void SetLalMacroBoundaryMode(GlobalLalMacroBoundaryMode mode);

  /** Set the regional capacity policy used by the global spreader. */
  void SetCapacityModel(std::shared_ptr<PlacementCapacityModel> capacity_model);

  /** Install an optional periodic physical upper-bound refiner. */
  void SetUpperBoundRefiner(
      std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner,
      int warmup_iteration, int interval);

  /** Select whether a refined physical upper bound becomes the next anchor. */
  void SetUseRefinedUpperBoundAsAnchor(bool enable) {
    refinement_feedback_mode_ = enable ? GlobalRefinementFeedbackMode::kFull
                                       : GlobalRefinementFeedbackMode::kNone;
  }

  /** Select which refined coordinate axes anchor the next analytical solve. */
  void SetRefinementFeedbackMode(GlobalRefinementFeedbackMode mode) {
    refinement_feedback_mode_ = mode;
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
  double convergence_gap_threshold_ = 0.08;
  int convergence_criteria_ = 1;
  // Stop only after best legalized HPWL has not improved by at least 0.2%
  // for several iterations while the lower/upper gap is already small.
  double upper_bound_min_improvement_ = 0.002;
  int upper_bound_improvement_patience_ = 5;

  // Save intermediate result for debugging and/or visualization.

  bool IsComponentListOrNetListEmpty() const;
  /** Return the relative improvement from old_value to new_value. */
  static double RelativeImprovement(double old_value, double new_value);

  /** Treat tiny floating-point values as zero for HPWL ratio checks. */
  static bool IsPositive(double value);

  /** Return true when legalized HPWL has not meaningfully improved recently. */
  bool HasUpperBoundHpwlStalled(
      const std::vector<double>& upper_bound_hpwl) const;

  bool UsesGriddedRoughLegalization() const;
  bool IsGriddedPlacementConverged() const;
  bool IsStandardCellPlacementConverged();
  bool IsPlacementConverged();
  /** Return true when this iteration has a valid bound for convergence. */
  bool HasCurrentConvergenceUpperBound() const;
  void PreparePlacement();
  void RunPlacementIterations();
  bool ShouldRefineUpperBound() const;
  /** Save the complete component state when the accepted upper bound improves.
   */
  void UpdateBestUpperBoundPlacement(double upper_bound_hpwl);
  /** Return a copy of all current component coordinates and orientations. */
  std::vector<ComponentLocation> SaveCurrentPlacement() const;
  /** Restore component coordinates and orientations from a placement copy. */
  void RestorePlacement(const std::vector<ComponentLocation>& placement);
  /** Restore the checkpoint requested by a destabilized physical refiner. */
  bool RollbackRefinementFeedbackIfRequested(
      const GlobalUpperBoundRefinement& refinement);
  /** Apply the configured refined coordinates to the next analytical anchor. */
  void ApplyRefinedAnchorFeedback(
      const std::vector<ComponentLocation>& placement_before_refinement,
      bool anchor_all_components, const std::vector<int>& component_ids,
      const std::vector<std::vector<int>>& component_rows);
  /** Return weighted Y HPWL for modeled nets incident to a component. */
  double ConnectedNetWeightedHpwlY(const Component& component) const;
  /** Return whether refined Y is no worse than the analytical Y locally. */
  bool IsRefinedYLocallyNonWorsening(Component& component,
                                     double analytical_y) const;
  /**
   * Build a combined row-scale Y target with non-increasing modeled HPWL.
   *
   * Candidates are ranked by gain from the analytical placement, then
   * rechecked as they are committed so interacting assignments cannot make
   * the aggregate feedback target worse. When positive gain is required,
   * neutral transactions are restored instead of becoming future anchors.
   * The optional baseline check also excludes moves that only become useful
   * because an earlier transaction changed their incident nets.
   */
  std::vector<bool> SelectTransactionalYFeedback(
      const std::vector<bool>& candidates,
      const std::vector<ComponentLocation>& analytical_placement,
      bool require_positive_gain, bool require_positive_baseline_gain) const;
  /** Build a linear-size chain of accepted physical row relationships. */
  std::vector<RelativeYConstraint> BuildRelativeYConstraints(
      const std::vector<std::vector<int>>& component_rows,
      const std::vector<bool>& accepted_components) const;
  /** Update LAL demand from physical pressure observed by the refiner. */
  void UpdateLegalizationPressure(const GlobalUpperBoundRefinement& refinement);
  /** Log displacement introduced by physical upper-bound refinement. */
  void LogRefinementDisplacement(
      const std::vector<ComponentLocation>& placement_before_refinement);
  /** Restore the lowest-HPWL accepted upper-bound placement. */
  void RestoreBestUpperBoundPlacement();
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& subgroup, int iteration);
  void EmitIterationSnapshot(const std::string& id_suffix,
                             const std::string& label_suffix,
                             const std::string& subgroup);
  void FinalizePlacement();
  const char* UpperBoundKindLabel() const;
  void PrintHpwl() const;
  void PrintEndStatement(std::string const& name_of_process,
                         bool is_success) override;

  PlacementInitializerType initializer_type_ =
      PlacementInitializerType::kUniform;
  int net_ignore_threshold_ = 100;
  GlobalLalExpansionMode lal_expansion_mode_ =
      GlobalLalExpansionMode::kSymmetric;
  GlobalLalHotspotMode lal_hotspot_mode_ = GlobalLalHotspotMode::kComponentArea;
  double lal_affine_scaling_weight_ = 0.65;
  GlobalLalMacroBoundaryMode lal_macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
  SnapshotCallback snapshot_callback_;
  std::shared_ptr<PlacementCapacityModel> capacity_model_ =
      std::make_shared<AreaCapacityModel>();
  std::unique_ptr<HpwlOptimizer> optimizer_;
  std::unique_ptr<GlobalSpreader> spreader_;
  std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner_;
  std::vector<double> accepted_upper_bound_hpwl_;
  // Only the entries of accepted_upper_bound_hpwl_ that are rough-legalized.
  std::vector<double> physical_upper_bound_hpwl_;
  /** Accepted physical upper-bound X HPWL for each global iteration. */
  std::vector<double> accepted_upper_bound_hpwl_x_;
  /** Accepted physical upper-bound Y HPWL for each global iteration. */
  std::vector<double> accepted_upper_bound_hpwl_y_;
  struct ComponentLocation {
    double lx = 0.0;
    double ly = 0.0;
    ComponentOrient orient = N;
  };
  std::vector<ComponentLocation> best_upper_bound_placement_;
  /** Analytical placement saved before the most recent anchor feedback. */
  std::vector<ComponentLocation> previous_feedback_checkpoint_;
  double best_upper_bound_hpwl_ = std::numeric_limits<double>::max();
  int upper_bound_refiner_warmup_ = 0;
  int upper_bound_refiner_interval_ = 1;
  GlobalRefinementFeedbackMode refinement_feedback_mode_ =
      GlobalRefinementFeedbackMode::kYRowTransactionalConsistent;
  bool current_upper_bound_is_physical_ = false;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
