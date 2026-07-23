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

/**
 * @file
 * Legalizes the cells of one row segment in X.
 *
 * A segment is a contiguous run of usable whitespace, so each is solved
 * independently: a row with blockages or taps becomes several small ordered
 * problems instead of one wide one. Cells keep their relative order, and the
 * optimizers minimize displacement from where each cell sat before
 * legalization.
 */
#include "row_segment.h"

#include <algorithm>
#include <cfloat>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/component_legalization_state.h"
#include "dali/placer/well_legalizer/component_segment.h"
#include "dali/placer/well_legalizer/optimization_helper.h"

namespace dali {

void RowSegment::SetLLX(int lx) { lx_ = lx; }

void RowSegment::SetURX(int ux) { lx_ = ux - width_; }

void RowSegment::SetWidth(int width) { width_ = width; }

void RowSegment::SetUsedSize(int used_size) { used_size_ = used_size; }

int RowSegment::LLX() const { return lx_; }

int RowSegment::URX() const { return lx_ + width_; }

int RowSegment::Width() const { return width_; }

int RowSegment::UsedSize() const { return used_size_; }

std::vector<ComponentRegion>& RowSegment::ComponentRegions() {
  return component_regions_;
}

/****
 * Add a given component to this segment.
 * Update used space and the component list
 * @param component_ptr: a pointer to the component which needs to be added to
 * this segment
 */
void RowSegment::AddComponentRegion(Component* component_ptr, int region_id) {
  used_size_ += component_ptr->Width();
  component_regions_.emplace_back(component_ptr, region_id);
}

void RowSegment::MinDisplacementLegalization(bool use_init_loc) {
  if (component_regions_.empty()) return;
  std::sort(component_regions_.begin(), component_regions_.end(),
            [](const ComponentRegion& br0, const ComponentRegion& br1) {
              return (br0.component->LLX() < br1.component->LLX()) ||
                     ((br0.component->LLX() == br1.component->LLX()) &&
                      (br0.component->Id() < br1.component->Id()));
            });

  std::vector<ComponentDisplacementVariable> vars;
  vars.reserve(component_regions_.size());
  if (use_init_loc) {
    for (auto& component_region : component_regions_) {
      auto aux_ptr = static_cast<ComponentLegalizationState*>(
          component_region.component->AuxPtr());
      vars.emplace_back(component_region.component->Width(),
                        aux_ptr->InitLoc().x, 1.0);
      vars.back().component_region = component_region;
    }
  } else {
    for (auto& component_region : component_regions_) {
      vars.emplace_back(component_region.component->Width(),
                        component_region.component->LLX(), 1.0);
      vars.back().component_region = component_region;
    }
  }

  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  for (auto& var : vars) {
    var.UpdateComponentLocation();
  }
}

void RowSegment::SnapComponentsToPlacementGrid() {
  for (auto& component_region : component_regions_) {
    component_region.component->SetLLX(
        std::round(component_region.component->LLX()));
  }
}

void RowSegment::SetOptimalAnchorWeight(double weight) {
  opt_anchor_weight_ = weight;
}

void RowSegment::FitInRange(std::vector<ComponentDisplacementVariable>& vars) {
  double left_contour = LLX();
  for (auto& var : vars) {
    if (var.Solution() < left_contour) {
      var.SetSolution(left_contour);
    }
    left_contour = var.Solution() + var.Width();
  }

  double right_contour = URX();
  for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
    auto& var = *it;
    int width = var.Width();
    double ux = var.Solution() + width;
    if (ux > right_contour) {
      var.SetSolution(right_contour - width);
    }
    right_contour = var.Solution();
  }
}

double RowSegment::DispCost(std::vector<ComponentDisplacementVariable>& vars,
                            int l, int r, bool is_linear) {
  double quadratic_disp = 0;
  for (int i = l; i <= r; ++i) {
    double disp = std::fabs(vars[i].Solution() - vars[i].InitX());
    if (is_linear) {
      quadratic_disp += vars[i].Weight();
    } else {
      quadratic_disp += vars[i].Weight() * disp * disp;
    }
  }
  return quadratic_disp;
}

void RowSegment::FindBestLocalOrder(
    std::vector<ComponentDisplacementVariable>& res, double& cost,
    std::vector<ComponentDisplacementVariable>& vars, int cur, int l, int r,
    double left_bound, double right_bound, double gap, int range,
    bool is_linear) {
  if (cur == r) {
    vars[l].SetSolution(left_bound);
    vars[r].SetSolution(right_bound - vars[r].Width());

    double left_contour = left_bound + gap + vars[l].Width();
    for (int i = l + 1; i < r; ++i) {
      vars[i].SetSolution(left_contour);
      left_contour += vars[i].Width() + gap;
    }

    double tmp_cost = DispCost(vars, l, r, is_linear);
    if (tmp_cost < cost) {
      cost = tmp_cost;
      for (int j = 0; j < range; ++j) {
        res[j] = vars[l + j];
      }
    }
  } else {
    // Permutations made
    for (int i = cur; i <= r; ++i) {
      // Swapping done
      std::swap(vars[cur], vars[i]);

      // Recursion called
      FindBestLocalOrder(res, cost, vars, cur + 1, l, r, left_bound,
                         right_bound, gap, range, is_linear);

      // backtrack
      std::swap(vars[cur], vars[i]);
    }
  }
}

void RowSegment::LocalReorder(std::vector<ComponentDisplacementVariable>& vars,
                              int range, int omit, bool is_linear) {
  int sz = static_cast<int>(vars.size());
  if (sz < range) return;

  int last_segment = sz - range - omit;
  ComponentDisplacementVariable tmp(0, 0, 0);
  std::vector<ComponentDisplacementVariable> res_local_order(range, tmp);
  for (int l = omit; l <= last_segment; ++l) {
    int total_component_width = 0;
    for (int j = 0; j < range; ++j) {
      res_local_order[j] = vars[l + j];
      total_component_width += res_local_order[j].Width();
    }
    int r = l + range - 1;
    double best_cost = DBL_MAX;
    double left_bound = vars[l].Solution();
    double right_bound = vars[r].Solution() + vars[r].Width();
    double gap =
        (right_bound - left_bound - total_component_width) / (range - 1);

    FindBestLocalOrder(res_local_order, best_cost, vars, l, l, r, left_bound,
                       right_bound, gap, range, is_linear);
    for (int j = 0; j < range; ++j) {
      vars[l + j] = res_local_order[j];
    }

    vars[l].SetSolution(left_bound);
    vars[r].SetSolution(right_bound - vars[r].Width());
    double left_contour = left_bound + vars[l].Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      vars[i].SetSolution(left_contour);
      left_contour += vars[i].Width() + gap;
    }
  }
}

void RowSegment::LocalReorder2(
    std::vector<ComponentDisplacementVariable>& vars) {
  int sz = static_cast<int>(vars.size());
  if (sz <= 2) return;
  for (int i = 0; i < sz - 1;) {
    // if a multideck cell
    if (vars[i].IsMultideckCell()) {
      if (vars[i].TendToRight()) {  // have a tendency to move right
        if (i + 1 < sz) {
          if ((vars[i + 1].IsMultideckCell() && vars[i + 1].TendToLeft()) ||
              (!vars[i + 1].IsMultideckCell())) {
            double left = vars[i].Solution();
            double right = vars[i + 1].Solution() + vars[i + 1].Width();
            vars[i].SetSolution(right - vars[i].Width());
            vars[i + 1].SetSolution(left);
            i += 1;
            continue;
          }
        }
      } else if (vars[i].TendToLeft()) {  // have a tendency to move left
        if (i - 1 > 0) {
          if ((vars[i - 1].IsMultideckCell() && vars[i - 1].TendToRight()) ||
              (!vars[i - 1].IsMultideckCell())) {
            double left = vars[i - 1].Solution();
            double right = vars[i].Solution() + vars[i].Width();
            vars[i - 1].SetSolution(right - vars[i - 1].Width());
            vars[i].SetSolution(left);
            i += 1;
            continue;
          }
        }
      }
    }
    ++i;
  }
}

std::vector<ComponentDisplacementVariable>
RowSegment::OptimizeQuadraticDisplacement(double lambda,
                                          bool is_weighted_anchor,
                                          bool is_reorder) {
  std::vector<ComponentDisplacementVariable> vars;
  if (component_regions_.empty()) return vars;

  std::sort(component_regions_.begin(), component_regions_.end(),
            [](const ComponentRegion& br0, const ComponentRegion& br1) {
              return (br0.component->LLX() < br1.component->LLX()) ||
                     ((br0.component->LLX() == br1.component->LLX()) &&
                      (br0.component->Id() < br1.component->Id()));
            });

  double ave_discrepancy = 1;
  if (is_weighted_anchor) {
    int sub_cell_cnt = 0;
    double sum_discrepancy = 0;
    for (auto& component_region : component_regions_) {
      Component* component_ptr = component_region.component;
      auto aux_ptr =
          static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
      double average_loc = aux_ptr->AverageLoc();
      double sub_loc = aux_ptr->SubLocs()[component_region.region_id];
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      sum_discrepancy += tmp_discrepancy;
      ++sub_cell_cnt;
    }
    ave_discrepancy = sum_discrepancy / sub_cell_cnt;
    if (ave_discrepancy < 1e-5) {
      ave_discrepancy = 1e-5;
    }
  }

  vars.reserve(component_regions_.size());
  for (auto& component_region : component_regions_) {
    Component* component_ptr = component_region.component;
    int region_cnt = component_ptr->MacroPtr()->RegionCount();
    auto aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    vars.emplace_back(component_ptr->Width(), aux_ptr->InitLoc().x,
                      lambda / region_cnt);
    vars.back().component_region = component_region;
    if (region_cnt <= 1) continue;
    double weight_discrepancy = 1;
    if (is_weighted_anchor) {
      double sub_loc = aux_ptr->SubLocs()[component_region.region_id];
      double average_loc = aux_ptr->AverageLoc();
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      weight_discrepancy = pow(1 + tmp_discrepancy / ave_discrepancy, 2.0);
    }
    double weight = (1 - lambda) * weight_discrepancy;
    vars.back().SetAnchor(aux_ptr->AverageLoc(), weight);
  }
  AbacusPlaceRow(vars);

  if (is_weighted_anchor) {
    FitInRange(vars);
    if (is_reorder) {
      LocalReorder(vars, 3, 0, false);
    }
  }

  return vars;
}

std::vector<ComponentDisplacementVariable>
RowSegment::OptimizeLinearDisplacement(double lambda, bool is_weighted_anchor,
                                       bool is_reorder) {
  std::vector<ComponentDisplacementVariable> vars;
  if (component_regions_.empty()) return vars;

  std::sort(component_regions_.begin(), component_regions_.end(),
            [](const ComponentRegion& br0, const ComponentRegion& br1) {
              return (br0.component->LLX() < br1.component->LLX()) ||
                     ((br0.component->LLX() == br1.component->LLX()) &&
                      (br0.component->Id() < br1.component->Id()));
            });

  double ave_discrepancy = 1;
  if (is_weighted_anchor) {
    int sub_cell_cnt = 0;
    double sum_discrepancy = 0;
    for (auto& component_region : component_regions_) {
      Component* component_ptr = component_region.component;
      auto aux_ptr =
          static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
      double average_loc = aux_ptr->AverageLoc();
      double sub_loc = aux_ptr->SubLocs()[component_region.region_id];
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      sum_discrepancy += tmp_discrepancy;
      ++sub_cell_cnt;
    }
    ave_discrepancy = sum_discrepancy / sub_cell_cnt;
    if (ave_discrepancy < 1e-5) {
      ave_discrepancy = 1e-5;
    }
  }

  vars.reserve(component_regions_.size());
  for (auto& component_region : component_regions_) {
    Component* component_ptr = component_region.component;
    int region_cnt = component_ptr->MacroPtr()->RegionCount();
    auto aux_ptr =
        static_cast<ComponentLegalizationState*>(component_ptr->AuxPtr());
    vars.emplace_back(component_ptr->Width(), aux_ptr->InitLoc().x,
                      lambda / region_cnt);
    vars.back().component_region = component_region;
    if (region_cnt <= 1) continue;
    double weight_discrepancy = 1;
    if (is_weighted_anchor) {
      double sub_loc = aux_ptr->SubLocs()[component_region.region_id];
      double average_loc = aux_ptr->AverageLoc();
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      weight_discrepancy = pow(1 + tmp_discrepancy / ave_discrepancy, 1.0);
    }
    vars.back().SetAnchor(aux_ptr->AverageLoc(),
                          (1 - lambda) * weight_discrepancy  // / region_cnt
    );
  }

  MinimizeLinearDisplacement(vars);

  if (is_weighted_anchor) {
    FitInRange(vars);
    if (is_reorder) LocalReorder(vars, 3, 0, true);
  }

  return vars;
}

}  // namespace dali
