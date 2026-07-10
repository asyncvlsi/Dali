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
#include <limits>
#include <utility>

#include "dali/common/logging.h"

namespace dali {

void StandardCellLegalizer::SetDisableCellFlip(bool disable_cell_flip) {
  disable_cell_flip_ = disable_cell_flip;
}

void StandardCellLegalizer::SetCostMode(
    StandardCellLegalizerCostMode cost_mode) {
  cost_mode_ = cost_mode;
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

  std::vector<std::pair<double, double>> original_locations;
  original_locations.reserve(components.size());
  for (const Component* component : components) {
    original_locations.emplace_back(component->LLX(), component->LLY());
  }

  bool is_success = AssignComponentsToSegments(components);
  if (is_success) {
    LegalizeAssignedSegments();
    ReportDisplacement(components, original_locations);
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
  assignment_indices_by_row_.clear();

  AddRowsFromPlacementBoundary();
  AddBlockagesFromCircuit();
  placement_model_.BuildFreeSegments();

  const auto& rows = placement_model_.Rows();
  assignment_indices_by_row_.resize(rows.size());
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
      assignment_indices_by_row_[row_index].push_back(
          static_cast<int>(segment_assignments_.size()) - 1);
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
              if (lhs->LLX() == rhs->LLX()) {
                return lhs->LLY() < rhs->LLY();
              }
              return lhs->LLX() < rhs->LLX();
            });
  return components;
}

bool StandardCellLegalizer::AssignComponentsToSegments(
    std::vector<Component*> components) {
  int failed_component_count = 0;
  for (Component* component : components) {
    std::vector<StandardCellRowLegalizationCell> legalized_cells;
    double x_displacement = 0.0;
    int assignment_index =
        FindBestSegment(*component, &legalized_cells, &x_displacement);
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
    assignment.cells = std::move(legalized_cells);
    assignment.x_displacement = x_displacement;
  }

  if (failed_component_count > 0) {
    LOG(error) << "Standard-cell legalizer failed to assign "
               << failed_component_count << " of " << components.size()
               << " movable components\n";
    return false;
  }
  return true;
}

int StandardCellLegalizer::FindBestSegment(
    Component& component,
    std::vector<StandardCellRowLegalizationCell>* legalized_cells,
    double* x_displacement) const {
  int best_assignment = -1;
  double best_cost = std::numeric_limits<double>::max();

  for (const auto& candidate : FindCandidateSegments(component)) {
    std::vector<StandardCellRowLegalizationCell> candidate_cells;
    double candidate_x_displacement = 0.0;
    double candidate_cost = 0.0;
    const auto& assignment = segment_assignments_[candidate.assignment_index];
    if (EvaluateCandidate(component, assignment, &candidate_cells,
                          &candidate_x_displacement, &candidate_cost) &&
        candidate_cost < best_cost) {
      best_assignment = candidate.assignment_index;
      best_cost = candidate_cost;
      *legalized_cells = std::move(candidate_cells);
      *x_displacement = candidate_x_displacement;
    }
  }

  return best_assignment;
}

std::vector<StandardCellLegalizer::AssignmentCandidate>
StandardCellLegalizer::FindCandidateSegments(Component& component) const {
  std::vector<AssignmentCandidate> candidates;
  candidates.reserve(kCandidateSegmentCount);

  const auto& rows = placement_model_.Rows();
  int target_ly = static_cast<int>(std::llround(component.LLY()));
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    double row_displacement =
        std::abs(rows[row_index].ly - target_ly) * ckt_ptr_->GridValueY();
    if (candidates.size() == kCandidateSegmentCount &&
        row_displacement > candidates.back().estimated_cost) {
      continue;
    }

    for (int assignment_index : assignment_indices_by_row_[row_index]) {
      const auto& assignment = segment_assignments_[assignment_index];
      if (assignment.remaining_width < component.Width()) {
        continue;
      }

      AssignmentCandidate candidate{assignment_index,
                                    CandidateCost(component, assignment)};
      auto insertion_point = std::lower_bound(
          candidates.begin(), candidates.end(), candidate,
          [](const AssignmentCandidate& lhs, const AssignmentCandidate& rhs) {
            return lhs.estimated_cost < rhs.estimated_cost;
          });
      candidates.insert(insertion_point, candidate);
      if (candidates.size() > kCandidateSegmentCount) {
        candidates.pop_back();
      }
    }
  }
  return candidates;
}

bool StandardCellLegalizer::EvaluateCandidate(
    Component& component, const SegmentAssignment& assignment,
    std::vector<StandardCellRowLegalizationCell>* legalized_cells,
    double* x_displacement, double* incremental_cost) const {
  const auto& row = placement_model_.Rows()[assignment.row_index];
  const auto& segment = row.free_segments[assignment.segment_index];

  *legalized_cells = assignment.cells;
  legalized_cells->push_back(
      {component.Id(), component.Width(), component.LLX(), 0});
  StandardCellRowLegalizer row_legalizer;
  if (!row_legalizer.Legalize(segment, row.site_width, legalized_cells)) {
    return false;
  }

  *x_displacement = 0.0;
  double component_legal_lx = component.LLX();
  for (const auto& cell : *legalized_cells) {
    *x_displacement += std::abs(cell.legal_lx - cell.target_lx);
    if (cell.id == component.Id()) {
      component_legal_lx = cell.legal_lx;
    }
  }
  double displacement_y =
      std::abs(row.ly - static_cast<int>(std::llround(component.LLY())));
  double displacement_cost =
      (*x_displacement - assignment.x_displacement) * ckt_ptr_->GridValueX() +
      displacement_y * ckt_ptr_->GridValueY();
  if (cost_mode_ == StandardCellLegalizerCostMode::kDisplacement) {
    *incremental_cost = displacement_cost;
    return true;
  }

  double wire_length_delta =
      ComponentWireLengthDelta(component, component_legal_lx, row.ly,
                               OrientForRow(assignment.row_index));
  *incremental_cost =
      wire_length_delta + kDisplacementTieBreakWeight * displacement_cost;
  return true;
}

double StandardCellLegalizer::NetWireLengthWithCandidate(
    Net& net, const Component& component, double candidate_lx,
    double candidate_ly, ComponentOrient candidate_orient) const {
  if (net.ComponentPins().size() <= 1) {
    return 0.0;
  }

  double min_x = std::numeric_limits<double>::max();
  double max_x = std::numeric_limits<double>::lowest();
  double min_y = std::numeric_limits<double>::max();
  double max_y = std::numeric_limits<double>::lowest();
  for (const NetPin& net_pin : net.ComponentPins()) {
    double pin_x = net_pin.AbsX();
    double pin_y = net_pin.AbsY();
    if (net_pin.ComponentId() == component.Id()) {
      pin_x = candidate_lx + net_pin.PinPtr()->OffsetX(candidate_orient);
      pin_y = candidate_ly + net_pin.PinPtr()->OffsetY(candidate_orient);
    }
    min_x = std::min(min_x, pin_x);
    max_x = std::max(max_x, pin_x);
    min_y = std::min(min_y, pin_y);
    max_y = std::max(max_y, pin_y);
  }

  return net.Weight() * ((max_x - min_x) * ckt_ptr_->GridValueX() +
                         (max_y - min_y) * ckt_ptr_->GridValueY());
}

double StandardCellLegalizer::ComponentWireLengthDelta(
    const Component& component, double candidate_lx, double candidate_ly,
    ComponentOrient candidate_orient) const {
  double delta = 0.0;
  for (int net_id : component.NetList()) {
    Net& net = ckt_ptr_->Nets()[net_id];
    double current_wire_length = net.WeightedHPWLX() * ckt_ptr_->GridValueX() +
                                 net.WeightedHPWLY() * ckt_ptr_->GridValueY();
    double candidate_wire_length = NetWireLengthWithCandidate(
        net, component, candidate_lx, candidate_ly, candidate_orient);
    delta += candidate_wire_length - current_wire_length;
  }
  return delta;
}

double StandardCellLegalizer::CandidateCost(
    Component& component, const SegmentAssignment& assignment) const {
  const auto& row = placement_model_.Rows()[assignment.row_index];
  const auto& segment = row.free_segments[assignment.segment_index];
  int target_lx = static_cast<int>(std::llround(component.LLX()));
  int legal_lx =
      std::max(segment.lx, std::min(segment.ux - component.Width(), target_lx));
  int displacement_x = std::abs(legal_lx - target_lx);
  int displacement_y =
      std::abs(row.ly - static_cast<int>(std::llround(component.LLY())));
  return displacement_x * ckt_ptr_->GridValueX() +
         displacement_y * ckt_ptr_->GridValueY();
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

void StandardCellLegalizer::ReportDisplacement(
    const std::vector<Component*>& components,
    const std::vector<std::pair<double, double>>& original_locations) const {
  DaliExpects(components.size() == original_locations.size(),
              "Component and original-location counts must match");

  double total_displacement = 0.0;
  double maximum_displacement = 0.0;
  for (size_t i = 0; i < components.size(); ++i) {
    double displacement =
        std::abs(components[i]->LLX() - original_locations[i].first) *
            ckt_ptr_->GridValueX() +
        std::abs(components[i]->LLY() - original_locations[i].second) *
            ckt_ptr_->GridValueY();
    total_displacement += displacement;
    maximum_displacement = std::max(maximum_displacement, displacement);
  }

  LOG(info) << "Standard-cell legalization displacement\n"
            << "  total: " << total_displacement << " um\n"
            << "  average: " << total_displacement / components.size()
            << " um\n"
            << "  maximum: " << maximum_displacement << " um\n";
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
