/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#include "dali/placer/well_legalizer/gridded_row_location_optimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_set>
#include <utility>

#include "dali/common/misc.h"

namespace dali {

std::vector<GriddedRowLocationOptimizer::RowGroup>
GriddedRowLocationOptimizer::CollectRowGroups(Stripe* stripe) const {
  auto& rows = stripe->gridded_rows_;
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow& lhs, const GriddedRow& rhs) {
              return lhs.LLY() < rhs.LLY();
            });

  std::vector<RowGroup> groups;
  for (int first = 0; first < static_cast<int>(rows.size());) {
    int last = first;
    while (last + 1 < static_cast<int>(rows.size()) &&
           rows[last].URY() == rows[last + 1].LLY()) {
      ++last;
    }

    groups.push_back({stripe, first, last});
    first = last + 1;
  }
  return groups;
}

bool GriddedRowLocationOptimizer::OptimizeGroup(const RowGroup& group) {
  auto& rows = group.stripe->gridded_rows_;
  int current_base = rows[group.first_row].LLY();
  int group_height = rows[group.last_row].URY() - current_base;
  int lower_bound = group.first_row == 0 ? group.stripe->LLY()
                                         : rows[group.first_row - 1].URY();
  int upper_edge = group.last_row + 1 == static_cast<int>(rows.size())
                       ? group.stripe->URY()
                       : rows[group.last_row + 1].LLY();
  int upper_bound = upper_edge - group_height;
  if (lower_bound >= upper_bound) return false;

  std::unordered_set<int> component_ids;
  std::set<int> net_ids;
  for (int row_id = group.first_row; row_id <= group.last_row; ++row_id) {
    for (Component* component : rows[row_id].Components()) {
      component_ids.insert(component->Id());
      net_ids.insert(component->NetList().begin(), component->NetList().end());
    }
  }

  std::vector<std::pair<double, double>> events;
  double total_weight = 0.0;
  for (int net_id : net_ids) {
    Net& net = circuit_->Nets()[net_id];
    double internal_min = std::numeric_limits<double>::max();
    double internal_max = std::numeric_limits<double>::lowest();
    double external_min = std::numeric_limits<double>::max();
    double external_max = std::numeric_limits<double>::lowest();
    for (const NetPin& pin : net.ComponentPins()) {
      double pin_y = pin.AbsY();
      if (component_ids.count(pin.ComponentId()) != 0) {
        internal_min = std::min(internal_min, pin_y - current_base);
        internal_max = std::max(internal_max, pin_y - current_base);
      } else {
        external_min = std::min(external_min, pin_y);
        external_max = std::max(external_max, pin_y);
      }
    }
    if (internal_min == std::numeric_limits<double>::max() ||
        external_min == std::numeric_limits<double>::max() ||
        net.Weight() <= 0.0) {
      continue;
    }

    double first_endpoint = external_min - internal_min;
    double second_endpoint = external_max - internal_max;
    events.emplace_back(std::min(first_endpoint, second_endpoint),
                        net.Weight());
    events.emplace_back(std::max(first_endpoint, second_endpoint),
                        net.Weight());
    total_weight += net.Weight();
  }
  if (events.empty()) return false;

  std::sort(events.begin(), events.end());
  double derivative = -total_weight;
  double target = current_base;
  for (size_t event_id = 0; event_id < events.size();) {
    target = events[event_id].first;
    while (event_id < events.size() && events[event_id].first == target) {
      derivative += events[event_id].second;
      ++event_id;
    }
    if (derivative >= 0.0) break;
  }
  target = std::clamp(target, static_cast<double>(lower_bound),
                      static_cast<double>(upper_bound));

  auto affected_cost = [this, &net_ids]() {
    double cost = 0.0;
    for (int net_id : net_ids) {
      cost += circuit_->Nets()[net_id].WeightedHPWLY();
    }
    return cost * circuit_->GridValueY();
  };
  auto move_group = [&rows, &group](int new_base) {
    int displacement = new_base - rows[group.first_row].LLY();
    for (int row_id = group.first_row; row_id <= group.last_row; ++row_id) {
      rows[row_id].SetLLY(rows[row_id].LLY() + displacement);
      rows[row_id].UpdateComponentLocY();
    }
  };

  double best_cost = affected_cost();
  int best_base = current_base;
  std::set<int> candidates = {
      static_cast<int>(std::floor(target)),
      static_cast<int>(std::ceil(target)),
      lower_bound,
      upper_bound,
  };
  for (int candidate : candidates) {
    candidate = std::clamp(candidate, lower_bound, upper_bound);
    move_group(candidate);
    double candidate_cost = affected_cost();
    if (candidate_cost + 1e-9 < best_cost) {
      best_cost = candidate_cost;
      best_base = candidate;
    }
    move_group(current_base);
  }

  if (best_base == current_base) return false;
  move_group(best_base);
  return true;
}

GriddedRowLocationResult GriddedRowLocationOptimizer::Optimize(
    std::vector<StripeColumn>* columns) {
  DaliExpects(circuit_ != nullptr,
              "Cannot optimize row locations without a circuit");
  DaliExpects(columns != nullptr,
              "Cannot optimize row locations without stripe columns");

  GriddedRowLocationResult result;
  result.hpwl_before = circuit_->WeightedHPWL();
  constexpr int kMaxSweeps = 4;
  for (int sweep = 0; sweep < kMaxSweeps; ++sweep) {
    int moved_this_sweep = 0;
    for (StripeColumn& column : *columns) {
      for (Stripe& stripe : column.stripe_list_) {
        std::vector<RowGroup> groups = CollectRowGroups(&stripe);
        result.groups_considered += static_cast<int>(groups.size());
        for (const RowGroup& group : groups) {
          if (OptimizeGroup(group)) {
            ++moved_this_sweep;
            ++result.groups_moved;
          }
        }
      }
    }
    ++result.sweeps;
    if (moved_this_sweep == 0) break;
  }
  result.hpwl_after = circuit_->WeightedHPWL();
  return result;
}

}  // namespace dali
