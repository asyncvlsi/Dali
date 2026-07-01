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
#include <cmath>
#include <numeric>
#include <unordered_set>

#include "dali/common/logging.h"

namespace dali {

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
    cost += ckt_ptr_->Nets()[net_id].WeightedHPWL();
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

bool DetailedPlacer::StartPlacement() {
  PrintStartStatement("detailed placement");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for detailed placement");

  double hpwl_before = WeightedHPWL();
  int reordered_windows = 0;
  int segment_count = 0;
  for (GeneralRow& row : ckt_ptr_->design().Rows()) {
    for (GeneralRowSegment& segment : row.RowSegments()) {
      ++segment_count;
      reordered_windows +=
          LocalReorderSegment(&segment, kLocalReorderWindowSize);
    }
  }

  double hpwl_after = WeightedHPWL();
  LOG(info) << "Detailed placement local re-ordering:\n"
            << "  row segments visited: " << segment_count << "\n"
            << "  accepted reorder windows: " << reordered_windows << "\n"
            << "  HPWL before: " << hpwl_before << "um\n"
            << "  HPWL after : " << hpwl_after << "um\n";

  PrintEndStatement("detailed placement", true);
  return true;
}

}  // namespace dali
