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

#include <vector>

#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_row_legalizer.h"
#include "dali/placer/placer.h"

namespace dali {

/**
 * Standard-cell legalizer built around row search and Abacus row placement.
 *
 * This is the standard-cell counterpart to the gridded/well legalization flow.
 * The initial implementation assigns cells to nearby free row segments, then
 * legalizes each segment with an Abacus-style row primitive. Negotiation and
 * rip-up/replacement can be layered on top of the same row model.
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
    std::vector<Component*> components;
    std::vector<StandardCellRowLegalizationCell> cells;
  };

  void BuildPlacementModel();
  void AddRowsFromPlacementBoundary();
  void AddBlockagesFromCircuit();
  std::vector<Component*> CollectMovableComponents();
  bool AssignComponentsToSegments(std::vector<Component*> components);
  int FindBestSegment(Component& component) const;
  int CandidateCost(Component& component,
                    const SegmentAssignment& assignment) const;
  void LegalizeAssignedSegments();
  void ExportRowsToCircuit();
  ComponentOrient OrientForRow(int row_index) const;

  StandardCellPlacementModel placement_model_;
  std::vector<SegmentAssignment> segment_assignments_;
  bool disable_cell_flip_ = false;
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_LEGALIZER_H_
