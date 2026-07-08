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
#ifndef DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_LEGALIZER_H_
#define DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_LEGALIZER_H_

#include <utility>
#include <vector>

#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_row_legalizer.h"
#include "dali/placer/placer.h"

namespace dali {

/**
 * Standard-cell legalizer built around row search and Abacus row placement.
 *
 * This is the standard-cell counterpart to the gridded/well legalization flow.
 * Cells are processed from left to right. A cheap physical-displacement search
 * identifies nearby segments, then trial Abacus insertions measure the packing
 * displacement induced in each shortlisted segment.
 */
class StandardCellLegalizer : public Placer {
 public:
  StandardCellLegalizer() = default;

  /** Disable orientation flipping during legalization. */
  void SetDisableCellFlip(bool disable_cell_flip);

  /** Run standard-cell legalization. */
  bool StartPlacement() override;

 private:
  struct SegmentAssignment {
    int row_index = -1;
    int segment_index = -1;
    int remaining_width = 0;
    double x_displacement = 0.0;
    std::vector<Component*> components;
    std::vector<StandardCellRowLegalizationCell> cells;
  };

  struct AssignmentCandidate {
    int assignment_index = -1;
    double estimated_cost = 0.0;
  };

  void BuildPlacementModel();
  void AddRowsFromPlacementBoundary();
  void AddBlockagesFromCircuit();
  std::vector<Component*> CollectMovableComponents();
  bool AssignComponentsToSegments(std::vector<Component*> components);
  int FindBestSegment(
      Component& component,
      std::vector<StandardCellRowLegalizationCell>* legalized_cells,
      double* x_displacement) const;

  /** Return the nearest feasible segments using unpacked displacement. */
  std::vector<AssignmentCandidate> FindCandidateSegments(
      Component& component) const;

  /** Trial-legalize one insertion and return its incremental displacement. */
  bool EvaluateCandidate(
      Component& component, const SegmentAssignment& assignment,
      std::vector<StandardCellRowLegalizationCell>* legalized_cells,
      double* x_displacement, double* incremental_cost) const;
  double CandidateCost(Component& component,
                       const SegmentAssignment& assignment) const;
  void LegalizeAssignedSegments();
  void ReportDisplacement(
      const std::vector<Component*>& components,
      const std::vector<std::pair<double, double>>& original_locations) const;
  void ExportRowsToCircuit();
  ComponentOrient OrientForRow(int row_index) const;

  StandardCellPlacementModel placement_model_;
  std::vector<SegmentAssignment> segment_assignments_;
  bool disable_cell_flip_ = false;

  static constexpr int kCandidateSegmentCount = 4;
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_LEGALIZER_H_
