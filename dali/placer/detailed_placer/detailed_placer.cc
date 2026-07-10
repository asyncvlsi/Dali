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
#include "dali/placer/detailed_placer/detailed_placer.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numeric>
#include <set>
#include <unordered_set>
#include <utility>

#include "dali/common/logging.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_row_legalizer.h"

namespace dali {

void DetailedPlacer::SetSnapshotCallback(SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void DetailedPlacer::SetMaxOptimizationRounds(int max_optimization_rounds) {
  DaliExpects(max_optimization_rounds >= 0,
              "Detailed placement maximum rounds must be non-negative");
  max_optimization_rounds_ = max_optimization_rounds;
}

void DetailedPlacer::SetMaxMoveCandidatesPerRound(
    int max_move_candidates_per_round) {
  DaliExpects(max_move_candidates_per_round >= 0,
              "Detailed placement move candidate cap must be non-negative");
  max_move_candidates_per_round_ = max_move_candidates_per_round;
}

double DetailedPlacer::WindowWireLengthCost(
    const std::vector<Component*>& components, int start, int window_size) {
  std::unordered_set<int> net_ids;
  for (int i = 0; i < window_size; ++i) {
    for (int net_id : components[start + i]->NetList()) {
      if (ckt_ptr_->Nets()[net_id].PinCnt() < 100) {
        net_ids.insert(net_id);
      }
    }
  }

  double cost = 0;
  for (int net_id : net_ids) {
    Net& net = ckt_ptr_->Nets()[net_id];
    cost += net.WeightedHPWLX() * ckt_ptr_->GridValueX() +
            net.WeightedHPWLY() * ckt_ptr_->GridValueY();
  }
  return cost;
}

void DetailedPlacer::PlaceWindow(const std::vector<Component*>& order,
                                 int left_bound, int right_bound) {
  int total_width = 0;
  for (Component* component : order) {
    total_width += component->Width();
  }

  int gap = 0;
  if (order.size() > 1) {
    gap = (right_bound - left_bound - total_width) /
          static_cast<int>(order.size() - 1);
  }

  int left_contour = left_bound;
  for (size_t i = 0; i < order.size(); ++i) {
    Component* component = order[i];
    if (i + 1 == order.size()) {
      component->SetURX(right_bound);
    } else {
      component->SetLLX(left_contour);
      left_contour += component->Width() + gap;
    }
  }
}

bool DetailedPlacer::ReorderWindow(std::vector<Component*>* components,
                                   int start, int window_size) {
  std::vector<Component*> original_order(window_size, nullptr);
  for (int i = 0; i < window_size; ++i) {
    original_order[i] = (*components)[start + i];
  }

  int left_bound = static_cast<int>(std::round(original_order.front()->LLX()));
  int right_bound = static_cast<int>(std::round(original_order.back()->URX()));
  int total_width = 0;
  for (Component* component : original_order) {
    total_width += component->Width();
  }
  if (total_width > right_bound - left_bound) {
    return false;
  }

  std::vector<Component*> best_order = original_order;
  double best_cost = WindowWireLengthCost(*components, start, window_size);

  std::vector<int> permutation(window_size);
  std::iota(permutation.begin(), permutation.end(), 0);
  do {
    std::vector<Component*> candidate_order(window_size, nullptr);
    for (int i = 0; i < window_size; ++i) {
      candidate_order[i] = original_order[permutation[i]];
    }

    PlaceWindow(candidate_order, left_bound, right_bound);
    double candidate_cost =
        WindowWireLengthCost(candidate_order, 0, window_size);
    if (candidate_cost + 1e-9 < best_cost) {
      best_cost = candidate_cost;
      best_order = candidate_order;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));

  bool changed = best_order != original_order;
  PlaceWindow(best_order, left_bound, right_bound);
  for (int i = 0; i < window_size; ++i) {
    (*components)[start + i] = best_order[i];
  }
  return changed;
}

int DetailedPlacer::LocalReorderSegment(GeneralRowSegment* segment,
                                        int window_size) {
  std::vector<Component*>& components = segment->Components();
  if (static_cast<int>(components.size()) < window_size) {
    return 0;
  }

  segment->SortComponents();
  int reorder_count = 0;
  int last_start = static_cast<int>(components.size()) - window_size;
  for (int start = 0; start <= last_start; ++start) {
    if (ReorderWindow(&components, start, window_size)) {
      ++reorder_count;
    }
  }
  return reorder_count;
}

void DetailedPlacer::BuildSwapIndex() {
  auto& rows = ckt_ptr_->design().Rows();
  row_components_.assign(rows.size(), {});
  row_segments_.assign(rows.size(), {});
  component_rows_.assign(ckt_ptr_->Components().size(), nullptr);
  component_segments_.assign(ckt_ptr_->Components().size(), nullptr);

  for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
    GeneralRow& row = rows[row_index];
    for (GeneralRowSegment& segment : row.RowSegments()) {
      row_segments_[row_index].push_back(&segment);
      segment.SortComponents();
      for (Component* component : segment.Components()) {
        row_components_[row_index].push_back(component);
        component_rows_[component->Id()] = &row;
        component_segments_[component->Id()] = &segment;
      }
    }
    std::sort(row_components_[row_index].begin(),
              row_components_[row_index].end(),
              [](const Component* lhs, const Component* rhs) {
                return lhs->CenterX() < rhs->CenterX();
              });
  }
}

DetailedPlacer::OptimalRegion DetailedPlacer::ComputeOptimalRegion(
    Component* component) const {
  std::vector<double> x_bounds;
  std::vector<double> y_bounds;
  for (int net_id : component->NetList()) {
    Net& net = ckt_ptr_->Nets()[net_id];
    if (net.PinCnt() <= 1 || net.PinCnt() >= 100) {
      continue;
    }

    for (NetPin& component_pin : net.ComponentPins()) {
      if (component_pin.ComponentPtr() != component) {
        continue;
      }
      double min_x = DBL_MAX;
      double max_x = -DBL_MAX;
      double min_y = DBL_MAX;
      double max_y = -DBL_MAX;
      for (NetPin& pin : net.ComponentPins()) {
        if (pin.ComponentPtr() == component) {
          continue;
        }
        double pin_x = pin.ComponentPtr()->LLX() + pin.PinPtr()->OffsetX(N);
        double pin_y = pin.ComponentPtr()->LLY() + pin.PinPtr()->OffsetY(N);
        min_x = std::min(min_x, pin_x);
        max_x = std::max(max_x, pin_x);
        min_y = std::min(min_y, pin_y);
        max_y = std::max(max_y, pin_y);
      }
      if (min_x != DBL_MAX) {
        x_bounds.push_back(min_x - component_pin.PinPtr()->OffsetX(N));
        x_bounds.push_back(max_x - component_pin.PinPtr()->OffsetX(N));
        y_bounds.push_back(min_y - component_pin.PinPtr()->OffsetY(N));
        y_bounds.push_back(max_y - component_pin.PinPtr()->OffsetY(N));
      }
    }
  }
  if (x_bounds.empty()) {
    return {};
  }

  std::sort(x_bounds.begin(), x_bounds.end());
  std::sort(y_bounds.begin(), y_bounds.end());
  size_t lower = (x_bounds.size() - 1) / 2;
  size_t upper = x_bounds.size() / 2;
  return {true, x_bounds[lower], y_bounds[lower], x_bounds[upper],
          y_bounds[upper]};
}

double DetailedPlacer::BookshelfStyleWireLength(int net_id) const {
  Net& net = ckt_ptr_->Nets()[net_id];
  if (net.ComponentPins().size() <= 1) {
    return 0;
  }

  double min_x = DBL_MAX;
  double max_x = -DBL_MAX;
  double min_y = DBL_MAX;
  double max_y = -DBL_MAX;
  for (NetPin& pin : net.ComponentPins()) {
    double pin_x = pin.ComponentPtr()->LLX() + pin.PinPtr()->OffsetX(N);
    double pin_y = pin.ComponentPtr()->LLY() + pin.PinPtr()->OffsetY(N);
    min_x = std::min(min_x, pin_x);
    max_x = std::max(max_x, pin_x);
    min_y = std::min(min_y, pin_y);
    max_y = std::max(max_y, pin_y);
  }
  return ((max_x - min_x) * ckt_ptr_->GridValueX() +
          (max_y - min_y) * ckt_ptr_->GridValueY()) *
         net.Weight();
}

double DetailedPlacer::DistanceToRegion(double x, double y,
                                        const OptimalRegion& region) const {
  double dx =
      x < region.lx ? region.lx - x : (x > region.ux ? x - region.ux : 0);
  double dy =
      y < region.ly ? region.ly - y : (y > region.uy ? y - region.uy : 0);
  return dx + dy;
}

std::vector<int> DetailedPlacer::FindClosestRows(
    const OptimalRegion& region) const {
  const auto& rows = ckt_ptr_->design().Rows();
  auto first_above = std::lower_bound(
      rows.begin(), rows.end(), region.ly,
      [](const GeneralRow& row, double y) { return row.LY() < y; });
  int right = static_cast<int>(first_above - rows.begin());
  int left = right - 1;
  std::vector<int> result;
  result.reserve(kMaxOptimalRegionRows);

  auto distance = [&rows, &region](int row_index) {
    double y = rows[row_index].LY();
    return y < region.ly ? region.ly - y : (y > region.uy ? y - region.uy : 0);
  };
  while (result.size() < kMaxOptimalRegionRows &&
         (left >= 0 || right < static_cast<int>(rows.size()))) {
    if (left < 0) {
      result.push_back(right++);
    } else if (right >= static_cast<int>(rows.size())) {
      result.push_back(left--);
    } else if (distance(left) <= distance(right)) {
      result.push_back(left--);
    } else {
      result.push_back(right++);
    }
  }
  return result;
}

double DetailedPlacer::AffectedWireLength(const std::set<int>& net_ids) const {
  double cost = 0.0;
  for (int net_id : net_ids) {
    cost += BookshelfStyleWireLength(net_id);
  }
  return cost;
}

bool DetailedPlacer::LegalizeSegment(GeneralRowSegment* segment,
                                     GeneralRow* row) {
  std::vector<StandardCellRowLegalizationCell> cells;
  cells.reserve(segment->Components().size());
  int total_width = 0;
  for (Component* component : segment->Components()) {
    cells.push_back({component->Id(), component->Width(), component->LLX(), 0});
    total_width += component->Width();
  }
  if (total_width > segment->Width()) {
    return false;
  }

  StandardCellRowLegalizer legalizer;
  StandardCellFreeSegment free_segment{segment->LX(),
                                       segment->LX() + segment->Width()};
  if (!legalizer.Legalize(free_segment, ckt_ptr_->MinComponentWidth(),
                          &cells)) {
    return false;
  }
  for (const auto& cell : cells) {
    Component& component = ckt_ptr_->Components()[cell.id];
    component.SetLowerLeft(cell.legal_lx, row->LY());
    component.SetOrient(row->IsOrientN() ? N : FS);
  }
  segment->SortComponents();
  return true;
}

int DetailedPlacer::UsedWidth(GeneralRowSegment* segment) const {
  DaliExpects(segment != nullptr, "Cannot measure a null row segment");
  int used_width = 0;
  for (Component* component : segment->Components()) {
    used_width += component->Width();
  }
  return used_width;
}

size_t DetailedPlacer::RowIndex(GeneralRow* row) const {
  DaliExpects(row != nullptr, "Cannot index a null row");
  const auto& rows = ckt_ptr_->design().Rows();
  return static_cast<size_t>(row - &rows.front());
}

std::vector<GeneralRowSegment*> DetailedPlacer::FindClosestSegmentsInRow(
    int row_index, double target_x) const {
  std::vector<GeneralRowSegment*> segments = row_segments_[row_index];
  std::sort(
      segments.begin(), segments.end(),
      [target_x](const GeneralRowSegment* lhs, const GeneralRowSegment* rhs) {
        auto distance = [target_x](const GeneralRowSegment* segment) {
          if (target_x < segment->LX()) {
            return segment->LX() - target_x;
          }
          if (target_x > segment->UX()) {
            return target_x - segment->UX();
          }
          return 0.0;
        };
        return distance(lhs) < distance(rhs);
      });
  if (segments.size() > kMaxSegmentsPerRow) {
    segments.resize(kMaxSegmentsPerRow);
  }
  return segments;
}

bool DetailedPlacer::TryMove(Component* component, GeneralRow* target_row,
                             GeneralRowSegment* target_segment,
                             double target_lx) {
  GeneralRow* source_row = component_rows_[component->Id()];
  GeneralRowSegment* source_segment = component_segments_[component->Id()];
  if (source_row == nullptr || source_segment == nullptr ||
      target_row == nullptr || target_segment == nullptr ||
      source_segment == target_segment) {
    return false;
  }
  if (UsedWidth(target_segment) + component->Width() >
      target_segment->Width()) {
    return false;
  }

  struct ComponentPlacement {
    Component* component = nullptr;
    double lx = 0;
    double ly = 0;
    ComponentOrient orientation = N;
  };
  auto capture_placements = [](const std::vector<Component*>& components) {
    std::vector<ComponentPlacement> placements;
    placements.reserve(components.size());
    for (Component* placed_component : components) {
      placements.push_back({placed_component, placed_component->LLX(),
                            placed_component->LLY(),
                            placed_component->Orient()});
    }
    return placements;
  };
  auto restore_placements =
      [](const std::vector<ComponentPlacement>& placements) {
        for (const auto& placement : placements) {
          placement.component->SetLowerLeft(placement.lx, placement.ly);
          placement.component->SetOrient(placement.orientation);
        }
      };

  auto source_components = source_segment->Components();
  auto target_components = target_segment->Components();
  auto original_placements = capture_placements(source_components);
  auto target_placements = capture_placements(target_components);
  original_placements.insert(original_placements.end(),
                             target_placements.begin(),
                             target_placements.end());

  std::set<int> affected_net_ids;
  for (const auto& placement : original_placements) {
    affected_net_ids.insert(placement.component->NetList().begin(),
                            placement.component->NetList().end());
  }
  double cost_before = AffectedWireLength(affected_net_ids);

  // Trial placement: remove from the source, seed the target near the optimal
  // region, then let row legalization resolve overlaps and site alignment.
  auto source_it = std::find(source_segment->Components().begin(),
                             source_segment->Components().end(), component);
  DaliExpects(source_it != source_segment->Components().end(),
              "Move component is not in its source segment");
  source_segment->Components().erase(source_it);
  target_segment->Components().push_back(component);
  component->SetLowerLeft(target_lx, target_row->LY());
  component->SetOrient(target_row->IsOrientN() ? N : FS);

  bool legal = LegalizeSegment(source_segment, source_row);
  if (legal) {
    legal = LegalizeSegment(target_segment, target_row);
  }

  auto candidate_source_components = source_segment->Components();
  auto candidate_target_components = target_segment->Components();
  auto candidate_placements =
      legal ? capture_placements(candidate_source_components)
            : std::vector<ComponentPlacement>();
  if (legal) {
    auto placements = capture_placements(candidate_target_components);
    candidate_placements.insert(candidate_placements.end(), placements.begin(),
                                placements.end());
  }
  double cost_after = legal ? AffectedWireLength(affected_net_ids) : DBL_MAX;

  source_segment->Components() = source_components;
  target_segment->Components() = target_components;
  restore_placements(original_placements);

  if (cost_after + 1e-9 >= cost_before) {
    return false;
  }

  source_segment->Components() = candidate_source_components;
  target_segment->Components() = candidate_target_components;
  restore_placements(candidate_placements);

  auto& source_row_components = row_components_[RowIndex(source_row)];
  auto source_row_it = std::find(source_row_components.begin(),
                                 source_row_components.end(), component);
  DaliExpects(source_row_it != source_row_components.end(),
              "Move component is not in its indexed source row");
  source_row_components.erase(source_row_it);
  row_components_[RowIndex(target_row)].push_back(component);
  for (auto& components : row_components_) {
    std::sort(components.begin(), components.end(),
              [](const Component* lhs, const Component* rhs) {
                return lhs->CenterX() < rhs->CenterX();
              });
  }
  component_rows_[component->Id()] = target_row;
  component_segments_[component->Id()] = target_segment;
  return true;
}

bool DetailedPlacer::IsPromisingSwap(Component* first, Component* second,
                                     GeneralRow* first_row,
                                     GeneralRow* second_row) const {
  std::set<int> net_ids(first->NetList().begin(), first->NetList().end());
  net_ids.insert(second->NetList().begin(), second->NetList().end());
  double cost_before = AffectedWireLength(net_ids);

  double first_lx = first->LLX();
  double first_ly = first->LLY();
  ComponentOrient first_orientation = first->Orient();
  double second_lx = second->LLX();
  double second_ly = second->LLY();
  ComponentOrient second_orientation = second->Orient();
  first->SetLowerLeft(second_lx, second_row->LY());
  first->SetOrient(second_row->IsOrientN() ? N : FS);
  second->SetLowerLeft(first_lx, first_row->LY());
  second->SetOrient(first_row->IsOrientN() ? N : FS);
  double cost_after = AffectedWireLength(net_ids);
  first->SetLowerLeft(first_lx, first_ly);
  first->SetOrient(first_orientation);
  second->SetLowerLeft(second_lx, second_ly);
  second->SetOrient(second_orientation);

  return cost_after + 1e-9 < cost_before;
}

bool DetailedPlacer::TrySwap(Component* first, Component* second) {
  if (first == second) {
    return false;
  }
  GeneralRow* first_row = component_rows_[first->Id()];
  GeneralRow* second_row = component_rows_[second->Id()];
  GeneralRowSegment* first_segment = component_segments_[first->Id()];
  GeneralRowSegment* second_segment = component_segments_[second->Id()];
  if (first_row == nullptr || second_row == nullptr ||
      first_segment == nullptr || second_segment == nullptr) {
    return false;
  }
  if (!IsPromisingSwap(first, second, first_row, second_row)) {
    return false;
  }

  struct ComponentPlacement {
    Component* component = nullptr;
    double lx = 0;
    double ly = 0;
    ComponentOrient orientation = N;
  };
  auto capture_placements = [](const std::vector<Component*>& components) {
    std::vector<ComponentPlacement> placements;
    placements.reserve(components.size());
    for (Component* component : components) {
      placements.push_back(
          {component, component->LLX(), component->LLY(), component->Orient()});
    }
    return placements;
  };
  auto restore_placements =
      [](const std::vector<ComponentPlacement>& placements) {
        for (const auto& placement : placements) {
          placement.component->SetLowerLeft(placement.lx, placement.ly);
          placement.component->SetOrient(placement.orientation);
        }
      };

  double first_lx = first->LLX();
  double second_lx = second->LLX();
  auto first_components = first_segment->Components();
  auto second_components = first_segment == second_segment
                               ? first_components
                               : second_segment->Components();
  auto original_placements = capture_placements(first_components);
  if (first_segment != second_segment) {
    auto placements = capture_placements(second_components);
    original_placements.insert(original_placements.end(), placements.begin(),
                               placements.end());
  }

  auto replace_component = [](std::vector<Component*>* components,
                              Component* removed, Component* added) {
    auto it = std::find(components->begin(), components->end(), removed);
    DaliExpects(it != components->end(),
                "Swap component is not in its segment");
    *it = added;
  };
  if (first_segment == second_segment) {
    first->SetLLX(second_lx);
    second->SetLLX(first_lx);
  } else {
    replace_component(&first_segment->Components(), first, second);
    replace_component(&second_segment->Components(), second, first);
    first->SetLLX(second_lx);
    second->SetLLX(first_lx);
  }

  bool legal = LegalizeSegment(first_segment, first_row);
  if (legal && first_segment != second_segment) {
    legal = LegalizeSegment(second_segment, second_row);
  }

  std::set<int> affected_net_ids;
  if (legal) {
    for (const auto& placement : original_placements) {
      if (placement.component->LLX() != placement.lx ||
          placement.component->LLY() != placement.ly ||
          placement.component->Orient() != placement.orientation) {
        affected_net_ids.insert(placement.component->NetList().begin(),
                                placement.component->NetList().end());
      }
    }
  }
  auto candidate_first_components = first_segment->Components();
  auto candidate_second_components = first_segment == second_segment
                                         ? candidate_first_components
                                         : second_segment->Components();
  auto candidate_placements =
      legal ? capture_placements(candidate_first_components)
            : std::vector<ComponentPlacement>();
  if (legal && first_segment != second_segment) {
    auto placements = capture_placements(candidate_second_components);
    candidate_placements.insert(candidate_placements.end(), placements.begin(),
                                placements.end());
  }
  double cost_after = legal ? AffectedWireLength(affected_net_ids) : DBL_MAX;

  first_segment->Components() = first_components;
  if (first_segment != second_segment) {
    second_segment->Components() = second_components;
  }
  restore_placements(original_placements);
  double cost_before = legal ? AffectedWireLength(affected_net_ids) : DBL_MAX;

  if (cost_after + 1e-9 < cost_before) {
    first_segment->Components() = candidate_first_components;
    if (first_segment != second_segment) {
      second_segment->Components() = candidate_second_components;
    }
    restore_placements(candidate_placements);
    if (first_segment != second_segment) {
      auto row_index = [this](GeneralRow* row) {
        return static_cast<size_t>(row - &ckt_ptr_->design().Rows().front());
      };
      auto replace_row_component = [](std::vector<Component*>* components,
                                      Component* removed, Component* added) {
        auto it = std::find(components->begin(), components->end(), removed);
        DaliExpects(it != components->end(),
                    "Swap component is not in its indexed row");
        *it = added;
        std::sort(components->begin(), components->end(),
                  [](const Component* lhs, const Component* rhs) {
                    return lhs->CenterX() < rhs->CenterX();
                  });
      };
      replace_row_component(&row_components_[row_index(first_row)], first,
                            second);
      replace_row_component(&row_components_[row_index(second_row)], second,
                            first);
      component_rows_[first->Id()] = second_row;
      component_rows_[second->Id()] = first_row;
      component_segments_[first->Id()] = second_segment;
      component_segments_[second->Id()] = first_segment;
    }
    return true;
  }

  return false;
}

int DetailedPlacer::RunOptimalRegionMoves() {
  BuildSwapIndex();
  struct MoveCandidate {
    Component* component = nullptr;
    OptimalRegion region;
    double current_distance = 0;
  };

  std::vector<MoveCandidate> move_candidates;
  move_candidates.reserve(ckt_ptr_->Components().size());
  for (Component& component : ckt_ptr_->Components()) {
    if (!component.IsMovable() || component.NetList().empty()) {
      continue;
    }
    OptimalRegion region = ComputeOptimalRegion(&component);
    if (!region.valid) {
      continue;
    }
    double current_distance =
        DistanceToRegion(component.LLX(), component.LLY(), region);
    if (current_distance > 1e-9) {
      move_candidates.push_back({&component, region, current_distance});
    }
  }
  std::sort(move_candidates.begin(), move_candidates.end(),
            [](const MoveCandidate& lhs, const MoveCandidate& rhs) {
              return lhs.current_distance > rhs.current_distance;
            });
  if (move_candidates.size() >
      static_cast<size_t>(max_move_candidates_per_round_)) {
    move_candidates.resize(max_move_candidates_per_round_);
  }

  int accepted = 0;
  for (const MoveCandidate& candidate : move_candidates) {
    Component* component = candidate.component;
    double target_lx = (candidate.region.lx + candidate.region.ux) / 2.0;
    for (int row_index : FindClosestRows(candidate.region)) {
      bool moved = false;
      GeneralRow* row = &ckt_ptr_->design().Rows()[row_index];
      for (GeneralRowSegment* segment :
           FindClosestSegmentsInRow(row_index, target_lx)) {
        if (UsedWidth(segment) + component->Width() > segment->Width()) {
          continue;
        }
        double clamped_target_lx = std::max<double>(
            segment->LX(),
            std::min<double>(segment->UX() - component->Width(), target_lx));
        double target_distance =
            DistanceToRegion(clamped_target_lx, row->LY(), candidate.region);
        if (target_distance + 1e-9 >= candidate.current_distance) {
          continue;
        }
        if (TryMove(component, row, segment, clamped_target_lx)) {
          ++accepted;
          moved = true;
          break;
        }
      }
      if (moved) {
        break;
      }
    }
  }
  return accepted;
}

int DetailedPlacer::RunOptimalRegionSwaps() {
  BuildSwapIndex();
  int accepted = 0;
  for (Component& component : ckt_ptr_->Components()) {
    if (!component.IsMovable() || component.NetList().empty()) {
      continue;
    }
    OptimalRegion region = ComputeOptimalRegion(&component);
    if (!region.valid) {
      continue;
    }
    double current_distance =
        DistanceToRegion(component.LLX(), component.LLY(), region);
    if (current_distance <= 1e-9) {
      continue;
    }

    std::vector<int> candidate_rows = FindClosestRows(region);
    bool swapped = false;
    double target_x = (region.lx + region.ux) / 2.0;
    for (int row_index : candidate_rows) {
      if (swapped) {
        break;
      }
      auto& candidates = row_components_[row_index];
      auto lower =
          std::lower_bound(candidates.begin(), candidates.end(), target_x,
                           [](const Component* candidate, double x) {
                             return candidate->CenterX() < x;
                           });
      for (int candidate_id = 0;
           candidate_id < kMaxCandidatesPerRow && !swapped; ++candidate_id) {
        int offset = candidate_id == 0 ? 0 : (candidate_id == 1 ? -1 : 1);
        auto index = static_cast<int>(lower - candidates.begin()) + offset;
        if (index < 0 || index >= static_cast<int>(candidates.size())) {
          continue;
        }
        if (DistanceToRegion(candidates[index]->LLX(), candidates[index]->LLY(),
                             region) >= current_distance) {
          continue;
        }
        if (TrySwap(&component, candidates[index])) {
          ++accepted;
          swapped = true;
        }
      }
    }
  }
  return accepted;
}

int DetailedPlacer::RunSingleSegmentClustering() {
  int accepted_segments = 0;
  for (GeneralRow& row : ckt_ptr_->design().Rows()) {
    for (GeneralRowSegment& segment : row.RowSegments()) {
      if (segment.Components().empty()) {
        continue;
      }

      std::set<int> net_ids;
      std::vector<double> original_lx;
      std::vector<StandardCellRowLegalizationCell> cells;
      original_lx.reserve(segment.Components().size());
      cells.reserve(segment.Components().size());
      for (Component* component : segment.Components()) {
        original_lx.push_back(component->LLX());
        net_ids.insert(component->NetList().begin(),
                       component->NetList().end());
        OptimalRegion region = ComputeOptimalRegion(component);
        double target_lx =
            region.valid ? (region.lx + region.ux) / 2.0 : component->LLX();
        cells.push_back({component->Id(), component->Width(), target_lx, 0});
      }

      double cost_before = AffectedWireLength(net_ids);
      StandardCellRowLegalizer legalizer;
      StandardCellFreeSegment free_segment{segment.LX(), segment.UX()};
      bool legal = legalizer.Legalize(free_segment,
                                      ckt_ptr_->MinComponentWidth(), &cells);
      DaliExpects(legal, "A legal row segment failed clustering repack");
      for (const auto& cell : cells) {
        ckt_ptr_->Components()[cell.id].SetLLX(cell.legal_lx);
      }

      if (AffectedWireLength(net_ids) + 1e-9 < cost_before) {
        ++accepted_segments;
        segment.SortComponents();
      } else {
        for (size_t i = 0; i < segment.Components().size(); ++i) {
          segment.Components()[i]->SetLLX(original_lx[i]);
        }
      }
    }
  }
  return accepted_segments;
}

int DetailedPlacer::RunLocalReordering(int* visited_segment_count) {
  DaliExpects(visited_segment_count != nullptr,
              "Detailed placement requires a segment-count output");
  *visited_segment_count = 0;
  int reordered_windows = 0;
  for (GeneralRow& row : ckt_ptr_->design().Rows()) {
    for (GeneralRowSegment& segment : row.RowSegments()) {
      ++(*visited_segment_count);
      reordered_windows +=
          LocalReorderSegment(&segment, kLocalReorderWindowSize);
    }
  }
  return reordered_windows;
}

void DetailedPlacer::EmitSnapshot(const std::string& id,
                                  const std::string& label,
                                  const std::string& subgroup, int iteration) {
  if (!snapshot_callback_) return;
  snapshot_callback_(id, label, subgroup, iteration);
}

bool DetailedPlacer::StartPlacement() {
  PrintStartStatement("detailed placement");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for detailed placement");

  double hpwl_before = WeightedHPWL();
  double previous_hpwl = hpwl_before;
  for (int round = 0; round < max_optimization_rounds_; ++round) {
    int accepted_clusters_before = RunSingleSegmentClustering();
    EmitSnapshot("round_" + std::to_string(round) + ".after_initial_clusters",
                 "Detailed Placement Round " + std::to_string(round) +
                     " After Initial Clustering",
                 "initial_clustering", round);
    int accepted_moves = RunOptimalRegionMoves();
    EmitSnapshot("round_" + std::to_string(round) + ".after_moves",
                 "Detailed Placement Round " + std::to_string(round) +
                     " After Optimal-Region Moves",
                 "optimal_region_moves", round);
    int accepted_swaps = RunOptimalRegionSwaps();
    double hpwl_after_swaps = WeightedHPWL();
    EmitSnapshot("round_" + std::to_string(round) + ".after_swaps",
                 "Detailed Placement Round " + std::to_string(round) +
                     " After Optimal-Region Swaps",
                 "optimal_region_swaps", round);
    int segment_count = 0;
    int reordered_windows = RunLocalReordering(&segment_count);
    EmitSnapshot("round_" + std::to_string(round) + ".after_reordering",
                 "Detailed Placement Round " + std::to_string(round) +
                     " After Local Reordering",
                 "local_reordering", round);
    int accepted_clusters_after = RunSingleSegmentClustering();
    double current_hpwl = WeightedHPWL();
    EmitSnapshot("round_" + std::to_string(round) + ".after_final_clusters",
                 "Detailed Placement Round " + std::to_string(round) +
                     " After Final Clustering",
                 "final_clustering", round);
    double relative_improvement =
        (previous_hpwl - current_hpwl) / previous_hpwl;

    LOG(info) << "  detailed placement round " << round << "\n"
              << "    accepted initial segment clusters: "
              << accepted_clusters_before << "\n"
              << "    accepted optimal-region moves: " << accepted_moves << "\n"
              << "    accepted optimal-region swaps: " << accepted_swaps << "\n"
              << "    row segments visited: " << segment_count << "\n"
              << "    accepted reorder windows: " << reordered_windows << "\n"
              << "    accepted final segment clusters: "
              << accepted_clusters_after << "\n"
              << "    HPWL after swaps: " << hpwl_after_swaps << "um\n"
              << "    HPWL after round: " << current_hpwl << "um\n"
              << "    relative improvement: " << relative_improvement << "\n";
    if (relative_improvement < kMinRelativeRoundImprovement) {
      break;
    }
    previous_hpwl = current_hpwl;
  }

  double hpwl_after = WeightedHPWL();
  LOG(info) << "Detailed placement summary:\n"
            << "  HPWL before: " << hpwl_before << "um\n"
            << "  HPWL after: " << hpwl_after << "um\n";

  PrintEndStatement("detailed placement", true);
  return true;
}

}  // namespace dali
