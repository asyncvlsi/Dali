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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_H_

#include <cfloat>
#include <unordered_map>

#include "dali/circuit/circuit.h"
#include "dali/circuit/component.h"
#include "dali/placer/well_legalizer/component_segment.h"
#include "dali/placer/well_legalizer/row_segment.h"

namespace dali {

/** Legalized row/cluster with gridded N/P-well structure. */
/**
 * One gridded row: a band of uniform well structure inside a stripe.
 *
 * Row height is set by the tallest cell the row holds, so rows are not on a
 * fixed pitch and their height is recomputed as cells are assigned. The row
 * owns its components and, once legalized in X, the row segments those
 * components are distributed across. Well taps are tracked separately in
 * `tap_cells_` because physical completion inserts them rather than the
 * assignment passes placing them.
 */
class GriddedRow {
  // clang-format off
  /*
   * Physical row completion uses this x-order:
   *
   *   [pre-end-cap margin][well tap][ordinary cells ...][well tap]
   *   [post-end-cap margin]
   *
   * The row legalizer packs ordinary cells between margins reserved for taps
   * and end caps. Boundary cells are placed after legalization and are
   * deliberately not added to components_.
   */
  // clang-format on
  friend class Stripe;

 public:
  GriddedRow() = default;

  /** Return true when row orientation is N. */
  bool IsOrientN() const;

  /** Return used x capacity in Dali grid units. */
  int UsedSize() const;

  /** Set used x capacity in Dali grid units. */
  void SetUsedSize(int used_size);

  /** Reserve additional row width. */
  void UseSpace(int width);

  /** Reserve left and right boundary widths for taps and end caps. */
  void SetBoundaryMargins(int left_margin, int right_margin);

  /** Return the left boundary width unavailable to ordinary components. */
  int LeftBoundaryMargin() const;

  /** Return the right boundary width unavailable to ordinary components. */
  int RightBoundaryMargin() const;

  /** Set lower-left x in Dali grid units. */
  void SetLLX(int lx);

  /** Set upper-right x in Dali grid units. */
  void SetURX(int ux);

  /** Return lower-left x in Dali grid units. */
  int LLX() const;

  /** Return upper-right x in Dali grid units. */
  int URX() const;

  /** Return center x in Dali grid units. */
  double CenterX() const;

  /** Set row width in Dali grid units. */
  void SetWidth(int width);

  /** Return row width in Dali grid units. */
  int Width() const;

  /** Return width available to ordinary components after boundary margins. */
  int UsableWidth() const;

  void SetLLY(int ly);
  void SetURY(int uy);
  int LLY() const;
  int URY() const;
  double CenterY() const;

  void SetHeight(int height);
  /**
   * Grow the row's well heights to fit a cell added from the bottom upward.
   *
   * A row is as tall as its tallest cell, so adding one may raise the P- or
   * N-well band. The upward and downward variants differ in which side the new
   * cell abuts and therefore which band grows.
   */
  void UpdateWellHeightUpward(int p_well_height, int n_well_height);
  /** Grow the well bands to fit a cell added from the top downward; the
   * counterpart of UpdateWellHeightUpward. */
  void UpdateWellHeightDownward(int p_well_height, int n_well_height);
  int Height() const;
  int PHeight() const;
  int NHeight() const;
  int PNEdge() const;

  /** Set the row's lower-left location. */
  void SetLoc(int lx, int ly);

  /** Assign a component to this row (order established by row assignment). */
  void AddComponent(Component* component_ptr);
  /** Return mutable ordinary components assigned to this row. */
  std::vector<Component*>& Components();
  /** Return ordinary components assigned to this row. */
  const std::vector<Component*>& Components() const;
  std::unordered_map<Component*, double2d>& InitLocations();
  /** Shift every component in the row by `x_disp` in X. */
  void ShiftComponentX(int x_disp);
  /** Shift every component in the row by `y_disp` in Y. */
  void ShiftComponentY(int y_disp);
  /** Shift every component in the row by `(x_disp, y_disp)`. */
  void ShiftComponent(int x_disp, int y_disp);
  void UpdateComponentLocY();
  /** Pack cells left-to-right with no gaps, in current order; the loose
   * counterpart LegalizeLooseX keeps cells near their targets instead. */
  void LegalizeCompactX(int left);
  void LegalizeCompactX();
  /**
   * Legalize ordinary cells within the row's usable x interval.
   *
   * The left and right margins remain untouched for boundary cells such as
   * end caps, which are inserted only after row legalization finishes.
   */
  void LegalizeLooseX();
  /** Synchronize row and component orientation, mirroring component Y once. */
  void SetOrient(bool is_orient_N);
  /** Insert a well tap with its center at the given X coordinate. */
  void InsertWellTapCell(Component& tap_cell, double center_x);

  /** Place a physical-completion cell at an exact X center coordinate. */
  void PlacePhysicalCell(Component& cell, double center_x) const;
  Component* WellTapCell() const;
  Component* LeftWellTapCell() const;
  Component* RightWellTapCell() const;

  /** All well-tap cells in this row, in insertion order, for any tap-placement
   * pattern (row-end pair, checkerboard, interior mini-row, etc.). */
  const std::vector<Component*>& TapCells() const { return tap_cells_; }

  void UpdateComponentLocationCompact();

  /**
   * Legally pack a fixed X-order with minimum unweighted squared displacement.
   *
   * This is the gridded-row equivalent of equal-weight Abacus clustering: it
   * preserves the order established by the row assignment, collapses
   * overlapping component clusters, and keeps ordinary cells inside the
   * usable interval after physical-completion margins are reserved.
   */
  void MinDisplacementLegalization();
  void UpdateMinDisplacementLLY();
  double MinDisplacementLLY() const;

  std::vector<RowSegment>& Segments();
  void UpdateSegments(std::vector<SegI>& blockage,
                      bool is_existing_components_considered);
  void AssignComponentsToSegments();
  /** Clustering predicate: is the component below the row's mid-line? The
   * iteration-parameterized variants widen the band as clustering proceeds. */
  bool IsBelowMiddleLine(Component* component) const;
  bool IsBelowTopPlusKFirstRegionHeight(Component* component,
                                        int iteration) const;
  /** Clustering predicate: is the component above the row's mid-line? */
  bool IsAboveMiddleLine(Component* component) const;
  bool IsAboveBottomMinusKFirstRegionHeight(Component* component,
                                            int iteration) const;
  /** Whether adding the component would overlap the row's contents. */
  bool IsOverlap(Component* component, int iteration, bool is_upward) const;

  /** Whether the component's orientation matches what this row's region needs. */
  bool IsOrientMatching(Component* component, int region_id) const;
  /** Record which of the component's well regions this row holds. */
  void AddComponentRegion(Component* component, int region_id, bool is_upward);
  std::vector<ComponentRegion>& ComponentRegions();
  /** Try to add a component, growing the row if it still fits.
   * @return true if it was added. */
  bool AttemptToAdd(Component* component, bool is_upward = true);
  bool AttemptToAddWithDispCheck(Component* component,
                                 double displacement_upper_limit,
                                 bool is_upward);
  ComponentOrient ComputeComponentOrient(Component* component,
                                         bool is_upward) const;
  /** Legalize each row segment in X independently.
   * @param use_init_loc measure displacement from the pre-legalization location. */
  void LegalizeSegmentsX(bool use_init_loc);
  void LegalizeSegmentsY();
  /** Recompute row height from its cells and the given well heights. */
  void RecomputeHeight(int p_well_height, int n_well_height);
  void InitializeComponentStretching();

  size_t AddWellTapCells(Circuit* p_ckt, Macro* well_tap_macro, size_t start_id,
                         std::vector<SegI>& well_tap_cell_locs);

  void SortComponentRegions();

  bool IsRowLegal();
  /** Return true when ordinary components fit between the reserved margins. */
  bool HasLegalComponentPlacement() const;
  /** Count overlapping component rectangles in this row. */
  size_t CountComponentOverlaps() const;

  void UpdateCommonSegment(std::vector<SegI>& avail_spaces, int width,
                           double density);
  /** Add a standard cell to a segment of this row over the given range. */
  void AddStandardCell(Component* component, int region_id, SegI range);

  size_t OutOfBoundCell();

 private:
  bool is_orient_N_ = true;             // orientation of this cluster
  std::vector<Component*> components_;  // list of components in this cluster
  std::unordered_map<Component*, double2d> initial_locations_;

  /**** number of tap cells needed, and pointers to tap cells ****/
  int tap_cell_num_ = 0;
  Component* tap_cell_ = nullptr;
  std::vector<Component*> tap_cells_;

  /**** x/y coordinates and dimension ****/
  int lx_ = 0;
  int ly_ = 0;
  int width_ = 0;

  /**** total width of cells in this cluster, including reserved space for tap
   * cells ****/
  int used_size_ = 0;
  int left_boundary_margin_ = 0;
  int right_boundary_margin_ = 0;

  /**** maximum p-well height and n-well height ****/
  int p_well_height_ = 0;
  int n_well_height_ = 0;
  int height_ = 0;

  /**** lly which gives minimal displacement ****/
  double min_displacement_lly_ = -DBL_MAX;

  /**** for multi-well legalization ****/
  std::vector<ComponentRegion> component_regions_;
  std::vector<RowSegment> segments_;
};

/** Vertical segment of one or more gridded rows. */
class VerticalRowSegment {
 public:
  VerticalRowSegment(GriddedRow* row, int location)
      : ly_(location), height_(row->Height()) {
    rows_.push_back(row);
  }

  int LY() const { return ly_; }
  int UY() const { return ly_ + height_; }
  int Height() const { return height_; }

  /** Return true when the next segment begins before this segment ends. */
  bool OverlapsNextRowSegment(const VerticalRowSegment& next_segment) const {
    return next_segment.LY() < UY();
  }

  /** Merge a following segment and recompute the minimum-displacement Y. */
  void Merge(const VerticalRowSegment& next_segment, int lower_bound,
             int upper_bound);

  /** Write contiguous segment locations back to its gridded rows. */
  void UpdateRowLocations();

 private:
  int ly_;
  int height_;
  std::vector<GriddedRow*> rows_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_H_
