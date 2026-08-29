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
#include "dali/placer/global_placer/placement_checkpoint.h"
#include "dali/placer/global_placer/placement_initializer.h"
#include "dali/placer/global_placer/stage_band.h"
#include "dali/placer/placer.h"

namespace dali {

/** Global placement flow combining initialization, HPWL optimization, and rough
 * legalization. */
class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  /** Timing of the mutually exclusive phases inside the iteration loop. */
  struct RuntimeBreakdown {
    double optimizer_wall_seconds = 0.0;
    double spreader_wall_seconds = 0.0;
    double shape_wall_seconds = 0.0;
    double physical_refinement_wall_seconds = 0.0;
    double timing_observer_wall_seconds = 0.0;
    double anchor_feedback_wall_seconds = 0.0;
    double other_wall_seconds = 0.0;
    int iterations = 0;
    int physical_refinements = 0;
    int timing_observer_calls = 0;
    int anchor_feedbacks = 0;

    double AccountedWallSeconds() const {
      return optimizer_wall_seconds + spreader_wall_seconds +
             shape_wall_seconds + physical_refinement_wall_seconds +
             timing_observer_wall_seconds + anchor_feedback_wall_seconds +
             other_wall_seconds;
    }
  };

  const RuntimeBreakdown& GetRuntimeBreakdown() const {
    return runtime_breakdown_;
  }

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

  /**
   * A delay line held in a fixed shape relative to its own first element.
   *
   * Offsets rather than absolute locations, because the shape has to ride along
   * with wherever wirelength optimization decides the delay line belongs. Only
   * the shape is imposed; the position is still the placer's to choose.
   *
   * Offsets are precomputed by the caller so no placement geometry lives here.
   */
  struct DelayLineShape {
    std::vector<int> component_ids;
    std::vector<double> offset_x;
    std::vector<double> offset_y;
  };

  /**
   * Hold delay lines in a spread shape across global placement iterations.
   *
   * Wirelength optimization abuts a chain of identical inverters, which is
   * exactly the delay a bundled-data slow path needs. Re-imposing the shape on
   * every upper bound makes it the target the anchor pseudo-nets pull toward,
   * so the spread is carried through the iterations instead of being competed
   * away by the objective that created the problem.
   */
  void SetDelayLineShapes(std::vector<DelayLineShape> shapes);

  /** The cells of one pipeline stage, to be held in one horizontal band. */
  struct StageBand {
    std::vector<int> component_ids;
  };

  /**
   * Hold each pipeline stage in a horizontal band across global placement.
   *
   * Bands are stacked in the order given, from the bottom of the placement
   * region upward, with heights proportional to their cell area. Like the
   * delay-line shapes, a band is re-imposed on every upper bound so that the
   * anchor pseudo-nets carry it into the next analytical solve; unlike them, it
   * constrains only the vertical axis, leaving the objective free to order
   * cells within a stage along the bit axis.
   *
   * Passing an empty vector removes the constraint.
   */
  void SetStageBands(std::vector<StageBand> bands);

  /** Choose equal-height bands over area-proportional ones. */
  void SetStageBandSpacing(StageBandSpacing spacing) {
    stage_band_spacing_ = spacing;
  }

  /**
   * Additionally reshape the spread upper bound to the bands.
   *
   * Off by default: the bands are expressed as terms in the quadratic problem,
   * and repositioning cells after the solve as well would be asserting the
   * answer on top of asking for it. Available because the spreader is free to
   * scatter a band that the solve had placed, and a design may need the
   * stronger form.
   */
  void SetStageBandReshapeUpperBound(bool enable) {
    stage_band_reshape_upper_bound_ = enable;
  }

  /** Re-evaluate timing mid-placement and return updated delay-line shapes. */
  using DelayLineFeedbackCallback =
      std::function<bool(int iteration, std::vector<DelayLineShape> *shapes)>;

  /**
   * Retune delay-line shapes from timing during the placement itself.
   *
   * Cheaper and better conditioned than re-placing per separation: the outer
   * form pays a full placement for every value tried and each run lands
   * somewhere slightly different, while retuning in place keeps one trajectory
   * and changes only the shape being carried.
   *
   * Timing is not consulted before `warmup` iterations, because early placements
   * have components still piled together and the resulting slack describes
   * nothing. It is consulted every `interval` iterations after that, and not at
   * all after `freeze` -- the remaining iterations then converge against a
   * shape that stops moving, which the convergence test needs.
   */
  void SetDelayLineFeedback(DelayLineFeedbackCallback callback, int warmup,
                            int interval, int freeze);

  /** Notify the owner after feedback shapes have been applied to the circuit. */
  using DelayLineFeedbackAppliedCallback = std::function<void(int iteration)>;
  void SetDelayLineFeedbackAppliedCallback(
      DelayLineFeedbackAppliedCallback callback);

  /**
   * Called with the accepted physical placement live, before anchor feedback.
   *
   * The accepted physical upper bound is the state checkpoint eligibility is
   * decided on, and it exists only for the few statements between the refiner
   * accepting it and ApplyRefinedAnchorFeedback restoring coordinates from the
   * analytical solve. Anything wanting to observe that state has to be handed
   * it here; by the time the iteration ends it is gone.
   *
   * Deliberately a bare iteration number. The placer does not know what the
   * observer intends to measure, and must not: keeping this generic is what
   * keeps delay-line policy out of GlobalPlacer.
   */
  using AcceptedPhysicalObserver = std::function<void(int iteration)>;
  /**
   * Observe the placement once the refined anchor feedback has been applied.
   *
   * A different state from the accepted physical one, not a later view of it:
   * the feedback restores coordinates from the analytical solve, so the two
   * disagree by construction. Amendment N found sizing reading this one while
   * believing it read the accepted upper bound, so both are now nameable.
   */
  void SetPostFeedbackObserver(AcceptedPhysicalObserver observer) {
    post_feedback_observer_ = std::move(observer);
  }

  void SetAcceptedPhysicalObserver(AcceptedPhysicalObserver observer) {
    accepted_physical_observer_ = std::move(observer);
  }

  /** Install an optional periodic physical upper-bound refiner. */
  void SetUpperBoundRefiner(
      std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner,
      int warmup_iteration, int interval);

  /**
   * Installs an observer consulted after every accepted physical upper bound.
   * The observer does not own placement: it may ask for the iteration loop to
   * pause so the caller can change topology, and the placer then rebuilds its
   * engines and resumes from the coordinates already reached.
   */
  void SetCheckpointObserver(PlacementCheckpointObserver *observer) {
    checkpoint_observer_ = observer;
  }
  int CheckpointRestarts() const { return checkpoint_restarts_; }

  /** Accepted upper-bound HPWL per iteration, spanning the whole run. */
  const std::vector<double> &AcceptedUpperBoundHpwls() const {
    return accepted_upper_bound_hpwl_;
  }

  /**
   * Whether any topology-sized placement engine is currently built.
   *
   * A host may only change the netlist while this is false. Exposed so that
   * can be asserted at the boundary rather than taken on trust.
   */
  bool ArePlacementEnginesOpen() const {
    return optimizer_ != nullptr || spreader_ != nullptr;
  }

  /**
   * Drop the caches that describe a component count the circuit no longer has.
   *
   * The in-loop checkpoint does this on its own path. A topology change made
   * after global placement has finished still leaves the same caches behind --
   * the best upper-bound placement and the feedback checkpoint are sized by the
   * netlist, and restoring either would abort on the size check. Exposed rather
   * than left private because the stage boundary is outside this class.
   */
  void ForgetTopologySizedCaches(size_t components_before) {
    InvalidateTopologySizedCaches(components_before);
  }

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
  PlacementCheckpointObserver *checkpoint_observer_ = nullptr;
  int checkpoint_restarts_ = 0;
  /** The checkpoint that asked placement to stop, pending its mutation. */
  PlacementCheckpoint pending_checkpoint_;
  /**
   * Whether the checkpointed iteration had already satisfied convergence.
   *
   * Recorded rather than inferred, because a checkpoint must not decide the
   * question by accident: the convergence test used to be skipped on a
   * checkpointed iteration, which silently added one.
   */
  bool converged_at_checkpoint_ = false;
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
  /** Runs iterations until convergence, the iteration cap, or a checkpoint. */
  bool RunPlacementIterations();
  /**
   * Hands the host its mutation window. Expects every topology-sized engine to
   * be closed already, and applies any delta it returns transactionally.
   */
  TopologyMutationStatus InvokeTopologyMutation();

  /** Clears state that belongs to the whole run, not to one topology. */
  void InitializeRunState();

  /** Drops caches whose size or ids the changed topology invalidated. */
  void InvalidateTopologySizedCaches(size_t components_before);
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
  /** Re-impose every registered delay-line shape on the current placement. */
  void ApplyDelayLineShapes();
  /** Map every registered stage's cells back into that stage's band. */
  void ApplyStageBands();
  /** Divide the region among the registered bands. */
  std::vector<StageBandInterval> StageBandIntervals() const;
  /** Aim each band's cells at that band for the next analytical solve. */
  void PublishStageBandAnchors();
  /** Ask for retuned delay-line shapes when this iteration is due. */
  void RefreshDelayLineShapes(int iteration);
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
  AcceptedPhysicalObserver accepted_physical_observer_;
  AcceptedPhysicalObserver post_feedback_observer_;
  std::shared_ptr<PlacementCapacityModel> capacity_model_ =
      std::make_shared<AreaCapacityModel>();
  std::unique_ptr<HpwlOptimizer> optimizer_;
  std::unique_ptr<GlobalSpreader> spreader_;
  std::vector<DelayLineShape> delay_line_shapes_;
  std::vector<StageBand> stage_bands_;
  StageBandSpacing stage_band_spacing_ = StageBandSpacing::kAreaProportional;
  bool stage_band_reshape_upper_bound_ = false;
  DelayLineFeedbackCallback delay_line_feedback_;
  DelayLineFeedbackAppliedCallback delay_line_feedback_applied_callback_;
  int delay_line_feedback_warmup_ = 0;
  int delay_line_feedback_interval_ = 1;
  int delay_line_feedback_freeze_ = 0;
  /**
   * Iteration at which a delay-line shape last changed, -1 if none has.
   *
   * Convergence is judged from a window of upper-bound HPWL, and a shape change
   * invalidates every sample taken before it, so the placer has to know where
   * that boundary is.
   */
  int last_delay_line_shape_change_iteration_ = -1;
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
  RuntimeBreakdown runtime_breakdown_;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
