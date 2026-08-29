/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/

/**
 * @file
 * Checks a finished gridded placement against the rules the flow must satisfy.
 *
 * `Validate` runs the structural checks -- rows within their stripe, components
 * within their row, no overlaps, orientation and Y consistency -- and reports
 * them together with the physical-completion checks for well taps and end caps.
 *
 * Well-tap coverage is pattern-agnostic on purpose: rather than assuming taps
 * sit at row ends or on any particular cadence, it measures the actual gap from
 * each cell to the nearest compatible tap and compares that against
 * MaxPlugDist. That is what lets a new tap pattern be validated without
 * teaching the validator about it.
 */
#include "dali/placer/well_legalizer/gridded_placement_validator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "dali/common/logging.h"

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

/**
 * Check the placement against every structural and physical rule and collect
 * the violations.
 * @return a report with per-rule counts; the placement is legal only when all
 *         are zero.
 */
GriddedPlacementLegalityReport GriddedPlacementValidator::Validate() const {
  constexpr double kCoordinateTolerance = 1e-9;
  constexpr size_t kMaxLoggedCoordinateViolations = 8;
  GriddedPlacementLegalityReport report;
  size_t logged_coordinate_violations = 0;
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
            if (logged_coordinate_violations < kMaxLoggedCoordinateViolations) {
              LOG(warning) << "Gridded-row Y violation: component="
                           << component->Name() << ", row=[" << row.LLY()
                           << ", " << row.URY()
                           << "], actual_y=" << component->LLY()
                           << ", expected_y=" << expected_y
                           << ", row_pn_height=" << row.PHeight() << "/"
                           << row.NHeight() << ", row_orientation="
                           << (row.IsOrientN() ? "N" : "FS") << ", orientation="
                           << (component->Orient() == N ? "N" : "FS") << "\n";
              ++logged_coordinate_violations;
            }
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
            // A sparse pattern may leave a row intentionally untapped; the
            // MaxPlugDist coverage check then guards that row's cells.
            if (config_.require_taps_every_row) {
              ++report.physical_completion_violation_count;
              ++report.missing_well_tap_count;
            }
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

    // Rows overlap only if they share area, so both axes have to be tested.
    // `ordered_rows` holds every row in the column, and a column carries a
    // stripe per band of x, so rows sitting side by side routinely share a y
    // range while being nowhere near each other. Ordering by y and comparing
    // neighbours reported those pairs as overlapping -- on the timing-driven
    // pipeline flows rows spanning x [518,586] and x [397,490] were counted as
    // overlapping because their y ranges met -- and legalization failed on
    // placements that were legal.
    //
    // Comparing neighbours is unsound here in the other direction too. Rows do
    // not all span their stripe: x ranges of [397,490], [518,586] and [397,586]
    // coexist, so no single ordering puts every pair that shares x adjacent,
    // and a genuine overlap could be hidden behind an intervening row. Each
    // pair that shares x is therefore tested directly.
    //
    // Sorting by x keeps that from being quadratic in practice: once a row
    // starts at or beyond the current row's right edge, no later row can reach
    // back, so the inner scan stops.
    std::sort(ordered_rows.begin(), ordered_rows.end(),
              [](const GriddedRow* lhs, const GriddedRow* rhs) {
                if (lhs->LLX() != rhs->LLX()) return lhs->LLX() < rhs->LLX();
                return lhs->LLY() < rhs->LLY();
              });
    for (size_t i = 0; i < ordered_rows.size(); ++i) {
      const GriddedRow* lhs = ordered_rows[i];
      for (size_t k = i + 1; k < ordered_rows.size(); ++k) {
        const GriddedRow* rhs = ordered_rows[k];
        if (rhs->LLX() >= lhs->URX()) break;
        if (lhs->LLX() >= rhs->URX()) continue;
        if (lhs->URY() <= rhs->LLY() || rhs->URY() <= lhs->LLY()) continue;
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

  if (config_.expect_well_taps && config_.check_exact_well_tap_count &&
      circuit_->design().WellTaps().size() !=
          static_cast<size_t>(config_.well_tap_count_per_row) * row_count) {
    ++report.physical_completion_violation_count;
    ++report.physical_component_count_violation_count;
  }
  if (config_.expect_end_caps &&
      circuit_->design().EndCapComponentCollection().GetSize() !=
          2 * row_count) {
    ++report.physical_completion_violation_count;
    ++report.physical_component_count_violation_count;
  }

  if (config_.check_well_tap_coverage) {
    ValidateWellTapCoverage(report);
  }
  return report;
}

/**
 * Measure each cell's distance to the nearest compatible tap against MaxPlugDist.
 *
 * Pattern-agnostic on purpose: it measures geometry rather than assuming where a
 * pattern places taps, so a new pattern needs no change here.
 * @param report receives the coverage violation count and worst gap.
 */
void GriddedPlacementValidator::ValidateWellTapCoverage(
    GriddedPlacementLegalityReport& report) const {
  constexpr double kCoordinateTolerance = 1e-9;
  constexpr size_t kMaxLoggedCoverageViolations = 8;

  double max_plug_dist = config_.max_plug_dist;
  if (max_plug_dist <= 0.0) {
    max_plug_dist = circuit_->tech().NwellLayer().MaxPlugDist();
  }
  if (max_plug_dist <= 0.0) {
    // No latch-up rule available; nothing to verify.
    return;
  }
  const double grid_value_x = circuit_->GridValueX();
  const double grid_value_y = circuit_->GridValueY();
  const double budget = max_plug_dist + kCoordinateTolerance;

  size_t logged = 0;
  // Wells are continuous within a stripe column, so a cell can be covered by any
  // tap in the same stripe regardless of which row that tap sits in. Grouping by
  // stripe keeps the check pattern-agnostic while respecting well locality.
  for (const StripeColumn& column : *columns_) {
    for (const Stripe& stripe : column.stripe_list_) {
      std::vector<std::pair<double, double>> tap_centers;  // microns
      for (const GriddedRow& row : stripe.gridded_rows_) {
        for (const Component* tap : row.TapCells()) {
          if (tap == nullptr) continue;
          tap_centers.emplace_back(
              0.5 * (tap->LLX() + tap->URX()) * grid_value_x,
              0.5 * (tap->LLY() + tap->URY()) * grid_value_y);
        }
      }
      if (tap_centers.empty()) continue;  // missing taps flagged elsewhere

      for (const GriddedRow& row : stripe.gridded_rows_) {
        for (const Component* component : row.Components()) {
          if (component == nullptr || !component->IsMovable()) continue;
          const double cx =
              0.5 * (component->LLX() + component->URX()) * grid_value_x;
          const double cy =
              0.5 * (component->LLY() + component->URY()) * grid_value_y;
          double nearest = std::numeric_limits<double>::max();
          for (const auto& tap : tap_centers) {
            const double dx = cx - tap.first;
            const double dy = cy - tap.second;
            nearest = std::min(nearest, std::hypot(dx, dy));
          }
          report.max_well_tap_coverage_gap =
              std::max(report.max_well_tap_coverage_gap, nearest);
          if (nearest > budget) {
            ++report.physical_completion_violation_count;
            ++report.well_tap_coverage_violation_count;
            if (logged < kMaxLoggedCoverageViolations) {
              LOG(error) << "Well-tap coverage violation (latch-up rule): "
                         << "component=" << component->Name() << " is "
                         << nearest << "um from the nearest well tap, exceeding "
                         << "MaxPlugDist=" << max_plug_dist
                         << "um; placement is not legal\n";
              ++logged;
            }
          }
        }
      }
    }
  }
}

}  // namespace dali
