/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_placement_validator.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dali {

size_t GriddedPlacementLegalityReport::TotalViolationCount() const {
  return unassigned_component_count + duplicate_assignment_count +
         invalid_component_reference_count + row_boundary_violation_count +
         row_overlap_count + component_boundary_violation_count +
         component_overlap_count + component_y_violation_count +
         component_orientation_violation_count +
         physical_completion_violation_count;
}

GriddedPlacementValidator::GriddedPlacementValidator(
    Circuit* circuit, const std::vector<StripeColumn>* columns,
    GriddedPlacementValidationConfig config)
    : circuit_(circuit), columns_(columns), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Gridded placement validation requires a circuit");
  DaliExpects(columns_ != nullptr,
              "Gridded placement validation requires stripe columns");
}

GriddedPlacementLegalityReport GriddedPlacementValidator::Validate() const {
  constexpr double kCoordinateTolerance = 1e-9;
  GriddedPlacementLegalityReport report;
  std::vector<Component>& components = circuit_->Components();
  std::vector<int> assignment_counts(components.size(), 0);
  std::vector<const GriddedRow*> ordered_rows;

  for (const Component& component : components) {
    if (component.IsMovable()) ++report.movable_component_count;
  }

  size_t row_count = 0;
  for (const StripeColumn& column : *columns_) {
    ordered_rows.clear();
    for (const Stripe& stripe : column.stripe_list_) {
      for (const GriddedRow& row : stripe.gridded_rows_) {
        ++row_count;
        ordered_rows.push_back(&row);
        if (row.LLX() < stripe.LLX() || row.URX() > stripe.URX() ||
            row.LLY() < stripe.LLY() || row.URY() > stripe.URY() ||
            row.LLX() < circuit_->RegionLLX() ||
            row.URX() > circuit_->RegionURX() ||
            row.LLY() < circuit_->RegionLLY() ||
            row.URY() > circuit_->RegionURY()) {
          ++report.row_boundary_violation_count;
        }
        if (!row.HasLegalComponentPlacement()) {
          ++report.component_boundary_violation_count;
        }
        report.component_overlap_count += row.CountComponentOverlaps();

        for (const Component* component : row.Components()) {
          if (component == nullptr || component->Id() < 0 ||
              component->Id() >= static_cast<int>(components.size()) ||
              component != &components[component->Id()] ||
              !component->IsMovable()) {
            ++report.invalid_component_reference_count;
            continue;
          }
          ++report.assigned_component_count;
          ++assignment_counts[component->Id()];

          const Macro* macro = component->MacroPtr();
          const double expected_y =
              row.IsOrientN()
                  ? row.LLY() + row.PHeight() - macro->FirstPwellHeight()
                  : row.LLY() + row.NHeight() - macro->FirstNwellHeight();
          if (std::fabs(component->LLY() - expected_y) > kCoordinateTolerance ||
              component->LLY() < row.LLY() - kCoordinateTolerance ||
              component->URY() > row.URY() + kCoordinateTolerance) {
            ++report.component_y_violation_count;
          }
          const ComponentOrient expected_orientation = row.IsOrientN() ? N : FS;
          if (config_.check_component_orientation &&
              component->Orient() != expected_orientation) {
            ++report.component_orientation_violation_count;
          }
        }

        if (config_.expect_well_taps) {
          const Component* left_tap = row.LeftWellTapCell();
          const Component* right_tap = row.RightWellTapCell();
          if (left_tap == nullptr || right_tap == nullptr) {
            ++report.physical_completion_violation_count;
            ++report.missing_well_tap_count;
          } else {
            const ComponentOrient expected_orientation =
                row.IsOrientN() ? N : FS;
            const double expected_left_lx =
                row.LLX() + config_.pre_end_cap_width;
            const double expected_right_ux =
                row.URX() - config_.post_end_cap_width;
            if (std::fabs(left_tap->LLX() - expected_left_lx) >
                    kCoordinateTolerance ||
                std::fabs(right_tap->URX() - expected_right_ux) >
                    kCoordinateTolerance ||
                left_tap->Orient() != expected_orientation ||
                right_tap->Orient() != expected_orientation ||
                left_tap->LLY() < row.LLY() || left_tap->URY() > row.URY() ||
                right_tap->LLY() < row.LLY() || right_tap->URY() > row.URY()) {
              ++report.physical_completion_violation_count;
              ++report.well_tap_geometry_violation_count;
            }
            for (const Component* component : row.Components()) {
              if (component->LLX() <
                      left_tap->URX() + config_.space_to_well_tap ||
                  component->URX() >
                      right_tap->LLX() - config_.space_to_well_tap) {
                ++report.physical_completion_violation_count;
                ++report.well_tap_spacing_violation_count;
                break;
              }
            }
          }
        }

        if (config_.expect_end_caps) {
          const auto& end_caps =
              circuit_->design().EndCapComponentCollection().Instances();
          const size_t left_index = 2 * (row_count - 1);
          const size_t right_index = left_index + 1;
          if (right_index >= end_caps.size()) {
            ++report.physical_completion_violation_count;
            ++report.missing_end_cap_count;
          } else {
            const Component& left_end_cap = end_caps[left_index];
            const Component& right_end_cap = end_caps[right_index];
            const ComponentOrient expected_orientation =
                row.IsOrientN() ? N : FS;
            if (std::fabs(left_end_cap.LLX() - row.LLX()) >
                    kCoordinateTolerance ||
                std::fabs(right_end_cap.URX() - row.URX()) >
                    kCoordinateTolerance ||
                left_end_cap.Orient() != expected_orientation ||
                right_end_cap.Orient() != expected_orientation ||
                left_end_cap.LLY() < row.LLY() ||
                left_end_cap.URY() > row.URY() ||
                right_end_cap.LLY() < row.LLY() ||
                right_end_cap.URY() > row.URY()) {
              ++report.physical_completion_violation_count;
              ++report.end_cap_geometry_violation_count;
            }
            if (config_.expect_well_taps && row.LeftWellTapCell() != nullptr &&
                row.RightWellTapCell() != nullptr &&
                (std::fabs(left_end_cap.URX() - row.LeftWellTapCell()->LLX()) >
                     kCoordinateTolerance ||
                 std::fabs(row.RightWellTapCell()->URX() -
                           right_end_cap.LLX()) > kCoordinateTolerance)) {
              ++report.physical_completion_violation_count;
              ++report.end_cap_tap_overlap_count;
            }
          }
        }
      }
    }

    std::sort(ordered_rows.begin(), ordered_rows.end(),
              [](const GriddedRow* lhs, const GriddedRow* rhs) {
                if (lhs->LLY() != rhs->LLY()) return lhs->LLY() < rhs->LLY();
                return lhs->LLX() < rhs->LLX();
              });
    for (size_t i = 1; i < ordered_rows.size(); ++i) {
      if (ordered_rows[i]->LLY() < ordered_rows[i - 1]->URY()) {
        ++report.row_overlap_count;
      }
    }
  }

  for (const Component& component : components) {
    if (!component.IsMovable()) continue;
    const int count = assignment_counts[component.Id()];
    if (count == 0) {
      ++report.unassigned_component_count;
    } else if (count > 1) {
      report.duplicate_assignment_count += count - 1;
    }
    if (component.LLX() < circuit_->RegionLLX() ||
        component.URX() > circuit_->RegionURX() ||
        component.LLY() < circuit_->RegionLLY() ||
        component.URY() > circuit_->RegionURY()) {
      ++report.component_boundary_violation_count;
    }
  }

  if (config_.expect_well_taps &&
      circuit_->design().WellTaps().size() != 2 * row_count) {
    ++report.physical_completion_violation_count;
    ++report.physical_component_count_violation_count;
  }
  if (config_.expect_end_caps &&
      circuit_->design().EndCapComponentCollection().GetSize() !=
          2 * row_count) {
    ++report.physical_completion_violation_count;
    ++report.physical_component_count_violation_count;
  }
  return report;
}

}  // namespace dali
