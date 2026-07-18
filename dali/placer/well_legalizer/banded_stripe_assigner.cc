/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/banded_stripe_assigner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#include "dali/common/helper.h"

namespace dali {

BandedStripeAssigner::BandedStripeAssigner(Circuit* circuit,
                                           BandedStripeAssignmentConfig config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr, "Banded stripe assignment needs a circuit");
  DaliExpects(config_.band_count > 0,
              "Banded stripe assignment needs at least one band");
  DaliExpects(config_.minimum_projected_hpwl_improvement >= 0.0,
              "Banded stripe HPWL margin cannot be negative");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Banded stripe net threshold must be at least two");
}

unsigned long long BandedStripeAssigner::BandCapacity(
    const StripeColumn& column, int band_lly, int band_ury) const {
  unsigned long long capacity = 0;
  for (const Stripe& stripe : column.stripe_list_) {
    const int overlap = std::max(
        0, std::min(band_ury, stripe.URY()) - std::max(band_lly, stripe.LLY()));
    const int usable_width =
        std::max(0, stripe.Width() - config_.capacity.reserved_width);
    capacity += static_cast<unsigned long long>(overlap) * usable_width;
  }
  return capacity;
}

double BandedStripeAssigner::ProjectedHpwlDelta(
    Component* component, const StripeColumn& target_column) const {
  double target_x = component->CenterX();
  double target_y = component->CenterY();
  double best_distance = std::numeric_limits<double>::infinity();
  for (const Stripe& stripe : target_column.stripe_list_) {
    if (component->Width() > stripe.Width()) continue;
    const double projected_x = std::clamp(
        component->CenterX(), stripe.LLX() + component->Width() / 2.0,
        stripe.URX() - component->Width() / 2.0);
    const double projected_y = std::clamp(
        component->CenterY(), stripe.LLY() + component->Height() / 2.0,
        stripe.URY() - component->Height() / 2.0);
    const double distance = std::abs(projected_x - component->CenterX()) +
                            std::abs(projected_y - component->CenterY());
    if (distance < best_distance) {
      best_distance = distance;
      target_x = projected_x;
      target_y = projected_y;
    }
  }
  if (!std::isfinite(best_distance)) {
    return std::numeric_limits<double>::infinity();
  }

  double hpwl_before = 0.0;
  for (int net_id : component->NetList()) {
    if (circuit_->Nets()[net_id].PinCnt() >=
        static_cast<size_t>(config_.net_ignore_threshold)) {
      continue;
    }
    hpwl_before += circuit_->NetWeightedHPWL(net_id);
  }
  const double original_x = component->CenterX();
  const double original_y = component->CenterY();
  component->SetCenterX(target_x);
  component->SetCenterY(target_y);
  double hpwl_after = 0.0;
  for (int net_id : component->NetList()) {
    if (circuit_->Nets()[net_id].PinCnt() >=
        static_cast<size_t>(config_.net_ignore_threshold)) {
      continue;
    }
    hpwl_after += circuit_->NetWeightedHPWL(net_id);
  }
  component->SetCenterX(original_x);
  component->SetCenterY(original_y);
  return hpwl_after - hpwl_before;
}

BandedStripeAssignmentResult BandedStripeAssigner::Assign(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr, "Cannot assign a null stripe collection");
  DaliExpects(!columns->empty(), "Cannot assign an empty stripe collection");

  DaliExpects(!columns->front().stripe_list_.empty(),
              "Banded stripe assignment needs stripe geometry");
  int region_bottom = columns->front().stripe_list_.front().LLY();
  int region_top = columns->front().stripe_list_.front().URY();
  for (const StripeColumn& column : *columns) {
    DaliExpects(!column.stripe_list_.empty(),
                "Banded stripe column has no stripe geometry");
    for (const Stripe& stripe : column.stripe_list_) {
      region_bottom = std::min(region_bottom, stripe.LLY());
      region_top = std::max(region_top, stripe.URY());
    }
  }
  DaliExpects(region_top > region_bottom,
              "Banded stripe assignment needs positive region height");

  std::unordered_map<int, int> original_column;
  for (int column_index = 0; column_index < static_cast<int>(columns->size());
       ++column_index) {
    for (Component* component : (*columns)[column_index].component_list_) {
      original_column[component->Id()] = column_index;
    }
  }

  std::vector<std::vector<Component*>> bands(config_.band_count);
  for (Component& component : circuit_->Components()) {
    if (!component.IsMovable()) continue;
    const double normalized_y =
        (component.CenterY() - region_bottom) / (region_top - region_bottom);
    const int band_index = std::clamp(
        static_cast<int>(std::floor(normalized_y * config_.band_count)), 0,
        config_.band_count - 1);
    bands[band_index].push_back(&component);
  }

  for (StripeColumn& column : *columns) {
    column.component_count_ = 0;
    column.component_list_.clear();
  }

  GriddedCapacityEstimator demand_estimator(config_.capacity);
  BandedStripeAssignmentResult result;
  long long total_column_displacement = 0;
  for (int band_index = 0; band_index < config_.band_count; ++band_index) {
    std::vector<Component*>& components = bands[band_index];
    if (components.empty()) continue;
    ++result.populated_band_count;
    std::sort(components.begin(), components.end(),
              [](const Component* lhs, const Component* rhs) {
                if (lhs->CenterX() != rhs->CenterX()) {
                  return lhs->CenterX() < rhs->CenterX();
                }
                return lhs->Id() < rhs->Id();
              });

    const int band_lly = region_bottom + (region_top - region_bottom) *
                                             band_index / config_.band_count;
    const int band_ury = region_bottom + (region_top - region_bottom) *
                                             (band_index + 1) /
                                             config_.band_count;
    std::vector<unsigned long long> capacities(columns->size(), 0);
    unsigned long long total_capacity = 0;
    for (int column_index = 0; column_index < static_cast<int>(columns->size());
         ++column_index) {
      capacities[column_index] =
          BandCapacity((*columns)[column_index], band_lly, band_ury);
      total_capacity += capacities[column_index];
    }
    DaliExpects(total_capacity > 0,
                "Populated assignment band has no stripe capacity");

    std::vector<unsigned long long> demands;
    demands.reserve(components.size());
    unsigned long long total_demand = 0;
    for (const Component* component : components) {
      const unsigned long long demand =
          demand_estimator.EstimateStandaloneDemand(*component);
      demands.push_back(demand);
      total_demand += demand;
    }

    std::vector<int> proposed_columns(components.size(), 0);
    int target_column = 0;
    unsigned long long cumulative_capacity = capacities.front();
    unsigned long long assigned_demand = 0;
    for (int component_index = 0;
         component_index < static_cast<int>(components.size());
         ++component_index) {
      const unsigned long long demand = demands[component_index];
      const long double demand_midpoint =
          static_cast<long double>(assigned_demand) + demand / 2.0L;
      while (target_column + 1 < static_cast<int>(columns->size()) &&
             demand_midpoint * total_capacity >
                 static_cast<long double>(cumulative_capacity) * total_demand) {
        ++target_column;
        cumulative_capacity += capacities[target_column];
      }

      proposed_columns[component_index] = target_column;
      assigned_demand += demand;
    }

    std::vector<int> final_columns(components.size(), 0);
    std::vector<unsigned long long> current_demand(columns->size(), 0);
    struct CandidateMove {
      int component_index = -1;
      int source_column = -1;
      int target_column = -1;
      double hpwl_delta = 0.0;
    };
    std::vector<CandidateMove> candidate_moves;
    for (int component_index = 0;
         component_index < static_cast<int>(components.size());
         ++component_index) {
      Component* component = components[component_index];
      const auto original = original_column.find(component->Id());
      const int source_column = original == original_column.end()
                                    ? proposed_columns[component_index]
                                    : original->second;
      final_columns[component_index] = source_column;
      current_demand[source_column] += demands[component_index];
      if (source_column == proposed_columns[component_index]) continue;
      ++result.proposed_move_count;
      candidate_moves.push_back(
          {component_index, source_column, proposed_columns[component_index],
           ProjectedHpwlDelta(component,
                              (*columns)[proposed_columns[component_index]])});
    }

    // Standalone well demand is intentionally conservative: it cannot be
    // compared directly with raw stripe area because cells share row heights
    // after legal clustering. Scale the available stripe fragments to this
    // band's total standalone demand, then enforce the resulting ownership
    // budget exactly. This prevents the previous gate from accepting a move
    // merely because the transport proposal itself had already overloaded the
    // target column.
    std::vector<unsigned long long> demand_budgets(columns->size(), 0);
    for (int column_index = 0; column_index < static_cast<int>(columns->size());
         ++column_index) {
      demand_budgets[column_index] = static_cast<unsigned long long>(
          static_cast<long double>(capacities[column_index]) * total_demand /
          total_capacity);
      if (current_demand[column_index] > demand_budgets[column_index]) {
        ++result.initially_overloaded_target_count;
      }
    }
    std::sort(candidate_moves.begin(), candidate_moves.end(),
              [](const CandidateMove& lhs, const CandidateMove& rhs) {
                if (lhs.hpwl_delta != rhs.hpwl_delta) {
                  return lhs.hpwl_delta < rhs.hpwl_delta;
                }
                return lhs.component_index < rhs.component_index;
              });
    for (const CandidateMove& move : candidate_moves) {
      if (config_.require_projected_hpwl_improvement &&
          !(move.hpwl_delta < -config_.minimum_projected_hpwl_improvement)) {
        ++result.rejected_hpwl_move_count;
        continue;
      }
      const unsigned long long demand = demands[move.component_index];
      if (current_demand[move.target_column] + demand >
          demand_budgets[move.target_column]) {
        ++result.rejected_capacity_move_count;
        continue;
      }
      current_demand[move.source_column] -= demand;
      current_demand[move.target_column] += demand;
      final_columns[move.component_index] = move.target_column;
      result.projected_hpwl_improvement -= move.hpwl_delta;
    }

    for (int component_index = 0;
         component_index < static_cast<int>(components.size());
         ++component_index) {
      Component* component = components[component_index];
      const int final_column = final_columns[component_index];
      (*columns)[final_column].component_list_.push_back(component);
      ++(*columns)[final_column].component_count_;
      ++result.assigned_component_count;
      const auto original = original_column.find(component->Id());
      if (original == original_column.end()) continue;
      const int displacement = std::abs(final_column - original->second);
      if (displacement > 0) ++result.moved_component_count;
      total_column_displacement += displacement;
      result.maximum_column_displacement =
          std::max(result.maximum_column_displacement, displacement);
    }
  }

  for (StripeColumn& column : *columns) column.AssignComponentToSimpleStripe();
  if (result.assigned_component_count > 0) {
    result.average_column_displacement =
        total_column_displacement /
        static_cast<double>(result.assigned_component_count);
  }
  return result;
}

}  // namespace dali
