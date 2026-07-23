/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/

/**
 * @file
 * Moves components out of stripes that do not fit.
 *
 * When rough legalization leaves a stripe over its height, this finds the
 * nearest stripe with room and estimates the wirelength cost of moving
 * candidates there, preferring the cheapest. It is a repair pass, not an
 * optimizer: the goal is a placement that fits, at the least wirelength cost
 * available.
 */
#include "dali/placer/well_legalizer/gridded_stripe_balancer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

GriddedStripeBalancer::GriddedStripeBalancer(Circuit* circuit,
                                             GriddedCapacityConfig config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr, "Stripe balancer requires a circuit");
  // Balancing addresses physical feasibility. Target-density whitespace is a
  // global-placement objective and must not be treated as unavailable here.
  config_.target_density = 1.0;
}

GriddedCapacityEstimate GriddedStripeBalancer::Estimate(
    const Stripe& stripe) const {
  unsigned long long whitespace =
      static_cast<unsigned long long>(stripe.Width()) * stripe.Height();
  return GriddedCapacityEstimator(config_).Estimate(
      stripe.component_ptrs_vec_, stripe.Width(), stripe.Height(), whitespace);
}

Stripe* GriddedStripeBalancer::FindNearestTarget(
    std::vector<StripeColumn>& columns, int source_column, Component* component,
    const std::unordered_map<Stripe*, unsigned long long>& available_spare)
    const {
  Stripe* best_target = nullptr;
  double best_distance = std::numeric_limits<double>::infinity();
  for (int column_distance = 1;
       column_distance < static_cast<int>(columns.size()); ++column_distance) {
    for (int direction : {-1, 1}) {
      int column_index = source_column + direction * column_distance;
      if (column_index < 0 ||
          column_index >= static_cast<int>(columns.size())) {
        continue;
      }
      for (Stripe& candidate_stripe : columns[column_index].stripe_list_) {
        Stripe* stripe = &candidate_stripe;
        if (component->Width() > stripe->Width()) continue;
        auto spare = available_spare.find(stripe);
        if (spare == available_spare.end() || spare->second == 0) continue;
        double dx = std::max({0.0, stripe->LLX() - component->CenterX(),
                              component->CenterX() - stripe->URX()});
        double dy = std::max({0.0, stripe->LLY() - component->CenterY(),
                              component->CenterY() - stripe->URY()});
        double distance = dx + dy;
        if (distance < best_distance) {
          best_distance = distance;
          best_target = stripe;
        }
      }
    }
    if (best_target != nullptr) break;
  }
  return best_target;
}

double GriddedStripeBalancer::EstimateAffectedNetHpwlDelta(
    Component* component, const Stripe& target) const {
  double original_x = component->CenterX();
  double original_y = component->CenterY();
  double target_x =
      std::clamp(original_x, target.LLX() + component->Width() / 2.0,
                 target.URX() - component->Width() / 2.0);
  double target_y =
      std::clamp(original_y, target.LLY() + component->Height() / 2.0,
                 target.URY() - component->Height() / 2.0);

  double hpwl_before = 0.0;
  for (int net_id : component->NetList()) {
    hpwl_before += circuit_->NetWeightedHPWL(net_id);
  }
  component->SetCenterX(target_x);
  component->SetCenterY(target_y);
  double hpwl_after = 0.0;
  for (int net_id : component->NetList()) {
    hpwl_after += circuit_->NetWeightedHPWL(net_id);
  }
  component->SetCenterX(original_x);
  component->SetCenterY(original_y);
  return hpwl_after - hpwl_before;
}

unsigned long long GriddedStripeBalancer::EstimateComponentDemand(
    const Component& component) const {
  const Macro* macro = component.MacroPtr();
  int p_height =
      std::max(macro->FirstPwellHeight(), config_.minimum_p_well_height);
  int n_height =
      std::max(macro->FirstNwellHeight(), config_.minimum_n_well_height);
  return static_cast<unsigned long long>(component.Width()) *
         (p_height + n_height);
}

GriddedStripeBalanceResult GriddedStripeBalancer::Balance(
    std::vector<StripeColumn>* stripe_columns) const {
  DaliExpects(stripe_columns != nullptr,
              "Cannot balance a null stripe collection");
  GriddedStripeBalanceResult result;
  std::unordered_map<Stripe*, unsigned long long> available_spare;
  std::unordered_map<Stripe*, unsigned long long> overflow_budget;
  for (StripeColumn& column : *stripe_columns) {
    for (Stripe& stripe : column.stripe_list_) {
      GriddedCapacityEstimate estimate = Estimate(stripe);
      if (estimate.predicted_overflow_area > 0) {
        ++result.overflowing_stripes_before;
        result.overflow_area_before += estimate.predicted_overflow_area;
        overflow_budget[&stripe] = estimate.predicted_overflow_area;
      } else {
        available_spare[&stripe] =
            estimate.available_gridded_area - estimate.required_gridded_area;
      }
    }
  }

  result.moved_component_count =
      ApplyMoves(stripe_columns, &overflow_budget, &available_spare,
                 &result.moved_component_ids);

  for (StripeColumn& column : *stripe_columns) {
    for (Stripe& stripe : column.stripe_list_) {
      GriddedCapacityEstimate estimate = Estimate(stripe);
      if (estimate.predicted_overflow_area > 0) {
        ++result.overflowing_stripes_after;
        result.overflow_area_after += estimate.predicted_overflow_area;
      }
    }
  }
  return result;
}

/**
 * Move components out of over-capacity stripes into the nearest with room.
 * @param stripe_columns updated in place.
 * @return the number of components moved.
 */
int GriddedStripeBalancer::ApplyMoves(
    std::vector<StripeColumn>* stripe_columns,
    std::unordered_map<Stripe*, unsigned long long>* overflow_budget,
    std::unordered_map<Stripe*, unsigned long long>* available_spare,
    std::vector<int>* moved_component_ids) const {
  DaliExpects(moved_component_ids != nullptr,
              "Cannot record stripe moves into a null component list");
  int moved_component_count = 0;

  for (int column_index = 0;
       column_index < static_cast<int>(stripe_columns->size());
       ++column_index) {
    for (Stripe& source : (*stripe_columns)[column_index].stripe_list_) {
      auto source_overflow = overflow_budget->find(&source);
      if (source_overflow == overflow_budget->end() ||
          source_overflow->second == 0) {
        continue;
      }

      std::vector<CandidateMove> candidates;
      candidates.reserve(source.component_ptrs_vec_.size());
      for (Component* component : source.component_ptrs_vec_) {
        Stripe* target = FindNearestTarget(*stripe_columns, column_index,
                                           component, *available_spare);
        if (target == nullptr) continue;
        unsigned long long demand = EstimateComponentDemand(*component);
        if ((*available_spare)[target] < demand) continue;
        double dx = std::max({0.0, target->LLX() - component->CenterX(),
                              component->CenterX() - target->URX()});
        double dy = std::max({0.0, target->LLY() - component->CenterY(),
                              component->CenterY() - target->URY()});
        candidates.push_back({component, &source, target,
                              EstimateAffectedNetHpwlDelta(component, *target),
                              dx + dy, demand});
      }
      std::sort(candidates.begin(), candidates.end(),
                [](const CandidateMove& lhs, const CandidateMove& rhs) {
                  if (lhs.hpwl_delta != rhs.hpwl_delta) {
                    return lhs.hpwl_delta < rhs.hpwl_delta;
                  }
                  return lhs.displacement < rhs.displacement;
                });

      unsigned long long relieved_area = 0;
      std::unordered_set<Component*> moved_from_source;
      for (const CandidateMove& candidate : candidates) {
        if (relieved_area >= source_overflow->second) break;
        if ((*available_spare)[candidate.target] < candidate.demand_area) {
          continue;
        }
        moved_from_source.insert(candidate.component);
        candidate.target->component_ptrs_vec_.push_back(candidate.component);
        (*available_spare)[candidate.target] -= candidate.demand_area;
        relieved_area += candidate.demand_area;
        moved_component_ids->push_back(candidate.component->Id());
        ++moved_component_count;
      }
      source.component_ptrs_vec_.erase(
          std::remove_if(source.component_ptrs_vec_.begin(),
                         source.component_ptrs_vec_.end(),
                         [&](Component* component) {
                           return moved_from_source.find(component) !=
                                  moved_from_source.end();
                         }),
          source.component_ptrs_vec_.end());
      source_overflow->second = relieved_area >= source_overflow->second
                                    ? 0
                                    : source_overflow->second - relieved_area;
    }
  }

  return moved_component_count;
}

GriddedStripeBalanceResult GriddedStripeBalancer::BalanceObservedOverflow(
    std::vector<StripeColumn>* stripe_columns) const {
  DaliExpects(stripe_columns != nullptr,
              "Cannot balance a null stripe collection");
  GriddedStripeBalanceResult result;
  std::unordered_map<Stripe*, unsigned long long> overflow_budget;
  std::unordered_map<Stripe*, unsigned long long> available_spare;
  for (StripeColumn& column : *stripe_columns) {
    for (Stripe& stripe : column.stripe_list_) {
      int usable_width = std::max(0, stripe.Width() - config_.reserved_width);
      if (stripe.used_height_ > stripe.Height()) {
        unsigned long long overflow =
            static_cast<unsigned long long>(stripe.used_height_ -
                                            stripe.Height()) *
            usable_width;
        overflow_budget[&stripe] = overflow;
        ++result.overflowing_stripes_before;
        result.overflow_area_before += overflow;
      } else {
        available_spare[&stripe] = static_cast<unsigned long long>(
                                       stripe.Height() - stripe.used_height_) *
                                   usable_width;
      }
    }
  }
  result.moved_component_count =
      ApplyMoves(stripe_columns, &overflow_budget, &available_spare,
                 &result.moved_component_ids);
  for (const auto& [stripe, overflow] : overflow_budget) {
    (void)stripe;
    if (overflow == 0) continue;
    ++result.overflowing_stripes_after;
    result.overflow_area_after += overflow;
  }
  return result;
}

}  // namespace dali
