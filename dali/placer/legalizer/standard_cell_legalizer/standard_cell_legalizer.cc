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
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_legalizer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "dali/common/logging.h"

namespace dali {

void StandardCellLegalizer::SetDisableCellFlip(bool disable_cell_flip) {
  disable_cell_flip_ = disable_cell_flip;
}

bool StandardCellLegalizer::StartPlacement() {
  PrintStartStatement("standard-cell legalization");

  BuildPlacementModel();
  auto components = CollectMovableComponents();
  if (components.empty()) {
    LOG(info) << "Skip standard-cell legalization: no movable components\n";
    PrintEndStatement("standard-cell legalization", true);
    return true;
  }

  bool is_success = AssignComponentsToSegments(components);
  if (is_success) {
    LegalizeAssignedSegments();
    ExportRowsToCircuit();
    UpdateMovableComponentPlacementStatus();
    ReportHPWL();
    ReportBoundingBox();
  }

  PrintEndStatement("standard-cell legalization", is_success);
  return is_success;
}

void StandardCellLegalizer::BuildPlacementModel() {
  placement_model_ = StandardCellPlacementModel();
  segment_assignments_.clear();

  AddRowsFromPlacementBoundary();
  AddBlockagesFromCircuit();
  placement_model_.BuildFreeSegments();

  const auto& rows = placement_model_.Rows();
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    const auto& row = rows[row_index];
    for (int segment_index = 0;
         segment_index < static_cast<int>(row.free_segments.size());
         ++segment_index) {
      const auto& segment = row.free_segments[segment_index];
      if (segment.Width() <= 0) {
        continue;
      }
      SegmentAssignment assignment;
      assignment.row_index = row_index;
      assignment.segment_index = segment_index;
      assignment.remaining_width = segment.Width();
      segment_assignments_.push_back(assignment);
    }
  }

  LOG(info) << "Standard-cell legalization row model\n"
            << "  rows: " << placement_model_.RowCount() << "\n"
            << "  free row segments: " << segment_assignments_.size() << "\n";
}

void StandardCellLegalizer::AddRowsFromPlacementBoundary() {
  int row_height = ckt_ptr_->RowHeightGridUnit();
  int row_count = RegionHeight() / row_height;
  int site_width = std::max(1, ckt_ptr_->MinComponentWidth());
  int site_count = RegionWidth() / site_width;

  for (int row_index = 0; row_index < row_count; ++row_index) {
    placement_model_.AddRow(RegionLeft(),
                            RegionBottom() + row_index * row_height, row_height,
                            site_width, site_count);
  }
}

void StandardCellLegalizer::AddBlockagesFromCircuit() {
  for (const auto& blockage : ckt_ptr_->design().PlacementBlockages()) {
    const auto& rect = blockage.GetRect();
    placement_model_.AddBlockage(rect.LLX(), rect.LLY(), rect.URX(),
                                 rect.URY());
  }

  for (const auto& component : ckt_ptr_->Components()) {
    if (component.IsMovable()) {
      continue;
    }
    placement_model_.AddBlockage(static_cast<int>(std::floor(component.LLX())),
                                 static_cast<int>(std::floor(component.LLY())),
                                 static_cast<int>(std::ceil(component.URX())),
                                 static_cast<int>(std::ceil(component.URY())));
  }
}

std::vector<Component*> StandardCellLegalizer::CollectMovableComponents() {
  std::vector<Component*> components;
  for (auto& component : ckt_ptr_->Components()) {
    if (IsDummyComponent(component) || component.IsFixed()) {
      continue;
    }
    components.push_back(&component);
  }

  std::sort(components.begin(), components.end(),
            [](const Component* lhs, const Component* rhs) {
              if (lhs->LLY() == rhs->LLY()) {
                return lhs->LLX() < rhs->LLX();
              }
              return lhs->LLY() < rhs->LLY();
            });
  return components;
}

bool StandardCellLegalizer::AssignComponentsToSegments(
    std::vector<Component*> components) {
  int failed_component_count = 0;
  for (Component* component : components) {
    int assignment_index = FindBestSegment(*component);
    if (assignment_index < 0) {
      ++failed_component_count;
      if (failed_component_count <= 5) {
        LOG(warning) << "Standard-cell legalizer could not assign component "
                     << component->Name() << " to any free row segment\n"
                     << "  component size(grid): " << component->Width()
                     << " x " << component->Height() << "\n"
                     << "  target loc(grid): (" << component->LLX() << ", "
                     << component->LLY() << ")\n";
      }
      continue;
    }

    auto& assignment = segment_assignments_[assignment_index];
    assignment.remaining_width -= component->Width();
    assignment.components.push_back(component);
    assignment.cells.push_back(
        {component->Id(), component->Width(), component->LLX(), 0});
  }

  if (failed_component_count > 0) {
    LOG(error) << "Standard-cell legalizer failed to assign "
               << failed_component_count << " of " << components.size()
               << " movable components\n";
    return false;
  }
  return true;
}

int StandardCellLegalizer::FindBestSegment(Component& component) const {
  int best_assignment = -1;
  int best_cost = 0;

  for (int assignment_index = 0;
       assignment_index < static_cast<int>(segment_assignments_.size());
       ++assignment_index) {
    const auto& assignment = segment_assignments_[assignment_index];
    if (assignment.remaining_width < component.Width()) {
      continue;
    }

    int cost = CandidateCost(component, assignment);
    if (best_assignment < 0 || cost < best_cost) {
      best_assignment = assignment_index;
      best_cost = cost;
    }
  }

  return best_assignment;
}

int StandardCellLegalizer::CandidateCost(
    Component& component, const SegmentAssignment& assignment) const {
  const auto& row = placement_model_.Rows()[assignment.row_index];
  const auto& segment = row.free_segments[assignment.segment_index];
  int target_lx = static_cast<int>(std::llround(component.LLX()));
  int legal_lx =
      std::max(segment.lx, std::min(segment.ux - component.Width(), target_lx));
  int displacement_x = std::abs(legal_lx - target_lx);
  int displacement_y =
      std::abs(row.ly - static_cast<int>(std::llround(component.LLY())));
  return displacement_x + displacement_y;
}

void StandardCellLegalizer::LegalizeAssignedSegments() {
  StandardCellRowLegalizer row_legalizer;
  for (auto& assignment : segment_assignments_) {
    if (assignment.components.empty()) {
      continue;
    }

    const auto& row = placement_model_.Rows()[assignment.row_index];
    const auto& segment = row.free_segments[assignment.segment_index];
    bool legal =
        row_legalizer.Legalize(segment, row.site_width, &assignment.cells);
    DaliExpects(legal, "Assigned cells overflow a standard-cell row segment");

    for (const auto& cell : assignment.cells) {
      DaliExpects(cell.id >= 0 &&
                      cell.id < static_cast<int>(ckt_ptr_->Components().size()),
                  "Standard-cell row legalizer returned an invalid component "
                  "id");
      Component* component = &ckt_ptr_->Components()[cell.id];
      component->SetLowerLeft(cell.legal_lx, row.ly);
      component->SetOrient(OrientForRow(assignment.row_index));
    }
  }
}

void StandardCellLegalizer::ExportRowsToCircuit() {
  auto& rows = ckt_ptr_->design().Rows();
  rows.clear();
  rows.reserve(placement_model_.Rows().size());

  for (int row_index = 0; row_index < placement_model_.RowCount();
       ++row_index) {
    const auto& model_row = placement_model_.Rows()[row_index];
    rows.emplace_back();
    GeneralRow& row = rows.back();
    row.SetLY(model_row.ly);
    row.SetHeight(model_row.height);
    row.SetOrient(OrientForRow(row_index) == N);

    for (const auto& segment : model_row.free_segments) {
      row.RowSegments().emplace_back();
      auto& row_segment = row.RowSegments().back();
      row_segment.SetLX(segment.lx);
      row_segment.SetWidth(segment.Width());
    }
  }

  for (auto& assignment : segment_assignments_) {
    if (assignment.components.empty()) {
      continue;
    }
    auto& row_segment =
        rows[assignment.row_index].RowSegments()[assignment.segment_index];
    for (Component* component : assignment.components) {
      row_segment.AddComponent(component);
    }
    row_segment.SortComponents();
  }
}

ComponentOrient StandardCellLegalizer::OrientForRow(int row_index) const {
  if (disable_cell_flip_) {
    return N;
  }
  return (row_index % 2 == 0) ? N : FS;
}

}  // namespace dali
