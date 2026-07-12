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
#include "gridded_row.h"

#include <algorithm>
#include <cfloat>

#include "dali/common/helper.h"

namespace dali {

bool GriddedRow::IsOrientN() const { return is_orient_N_; }

int GriddedRow::UsedSize() const { return used_size_; }

void GriddedRow::SetUsedSize(int used_size) { used_size_ = used_size; }

void GriddedRow::UseSpace(int width) { used_size_ += width; }

void GriddedRow::SetBoundaryMargins(int left_margin, int right_margin) {
  DaliExpects(left_margin >= 0 && right_margin >= 0,
              "Gridded-row boundary margins cannot be negative");
  left_boundary_margin_ = left_margin;
  right_boundary_margin_ = right_margin;
}

int GriddedRow::LeftBoundaryMargin() const { return left_boundary_margin_; }

int GriddedRow::RightBoundaryMargin() const { return right_boundary_margin_; }

void GriddedRow::SetLLX(int lx) { lx_ = lx; }

void GriddedRow::SetURX(int ux) { lx_ = ux - width_; }

int GriddedRow::LLX() const { return lx_; }

int GriddedRow::URX() const { return lx_ + width_; }

double GriddedRow::CenterX() const { return lx_ + width_ / 2.0; }

void GriddedRow::SetWidth(int width) { width_ = width; }

int GriddedRow::Width() const { return width_; }

void GriddedRow::SetLLY(int ly) { ly_ = ly; }

void GriddedRow::SetURY(int uy) { ly_ = uy - Height(); }

int GriddedRow::LLY() const { return ly_; }

int GriddedRow::URY() const { return ly_ + Height(); }

double GriddedRow::CenterY() const { return ly_ + height_ / 2.0; }

void GriddedRow::SetHeight(int height) { height_ = height; }

/****
 * Update the height of this cluster with the lower y of this cluster fixed.
 * So even if the height changes, the lower y of this cluster does not need be
 * changed.
 * ****/
void GriddedRow::UpdateWellHeightUpward(int p_well_height, int n_well_height) {
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
}

/****
 * Update the height of this cluster with the upper y of this cluster fixed.
 * So if the height changes, then the lower y of this cluster should also be
 * changed.
 * ****/
void GriddedRow::UpdateWellHeightDownward(int p_well_height,
                                          int n_well_height) {
  int old_height = height_;
  p_well_height_ = std::max(p_well_height_, p_well_height);
  n_well_height_ = std::max(n_well_height_, n_well_height);
  height_ = p_well_height_ + n_well_height_;
  ly_ -= (height_ - old_height);
}

int GriddedRow::Height() const { return height_; }

int GriddedRow::PHeight() const { return p_well_height_; }

int GriddedRow::NHeight() const { return n_well_height_; }

/****
 * Returns the P/N well edge to the bottom of this cluster
 * ****/
int GriddedRow::PNEdge() const { return is_orient_N_ ? PHeight() : NHeight(); }

void GriddedRow::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
}

void GriddedRow::AddComponent(Component* component_ptr) {
  components_.push_back(component_ptr);
  double y_init = component_ptr->LLY();
  Macro* macro_ptr = component_ptr->MacroPtr();
  y_init = component_ptr->LLY() + macro_ptr->FirstPwellHeight();
  initial_locations_[component_ptr] = double2d(component_ptr->LLX(), y_init);
}

std::vector<Component*>& GriddedRow::Components() { return components_; }

const std::vector<Component*>& GriddedRow::Components() const {
  return components_;
}

std::unordered_map<Component*, double2d>& GriddedRow::InitLocations() {
  return initial_locations_;
}

void GriddedRow::ShiftComponentX(int x_disp) {
  for (auto& component_ptr : components_) {
    component_ptr->IncreaseX(x_disp);
  }
}

void GriddedRow::ShiftComponentY(int y_disp) {
  for (auto& component_ptr : components_) {
    component_ptr->IncreaseY(y_disp);
  }
}

void GriddedRow::ShiftComponent(int x_disp, int y_disp) {
  for (auto& component_ptr : components_) {
    component_ptr->IncreaseX(x_disp);
    component_ptr->IncreaseY(y_disp);
  }
}

void GriddedRow::UpdateComponentLocY() {
  for (auto& component_ptr : components_) {
    Macro* macro_ptr = component_ptr->MacroPtr();
    if (is_orient_N_) {
      component_ptr->SetLLY(ly_ + p_well_height_ -
                            macro_ptr->FirstPwellHeight());
    } else {
      component_ptr->SetLLY(ly_ + n_well_height_ -
                            macro_ptr->FirstNwellHeight());
    }
  }
}

void GriddedRow::LegalizeCompactX(int left) {
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->LLX() < component_ptr1->LLX();
      });
  int current_x = left;
  for (auto& component : components_) {
    component->SetLLX(current_x);
    current_x += component->Width();
  }
}

void GriddedRow::LegalizeCompactX() {
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->LLX() < component_ptr1->LLX();
      });
  int current_x = lx_ + left_boundary_margin_;
  for (auto& component : components_) {
    component->SetLLX(current_x);
    current_x += component->Width();
  }
}

/****
 * Legalize this cluster using the extended Tetris legalization algorithm
 *
 * 1. legalize components from left
 * 2. if component contour goes out of the right boundary, legalize components
 * from right
 *
 * if the total width of components in this cluster is smaller than the width of
 * this cluster, two-rounds legalization is enough to make the final result
 * legal.
 * ****/
void GriddedRow::LegalizeLooseX() {
  if (components_.empty()) {
    return;
  }
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->LLX() < component_ptr1->LLX();
      });
  int component_contour = lx_ + left_boundary_margin_;
  int res_x;
  for (auto& component : components_) {
    res_x = std::max(component_contour, int(component->LLX()));
    component->SetLLX(res_x);
    component_contour = int(component->URX());
  }

  int ux = lx_ + width_ - right_boundary_margin_;
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->URX() > component_ptr1->URX();
      });
  component_contour = ux;
  for (auto& component : components_) {
    res_x = std::min(component_contour, int(component->URX()));
    component->SetURX(res_x);
    component_contour = int(component->LLX());
  }
}

void GriddedRow::SetOrient(bool is_orient_N) {
  if (is_orient_N_ != is_orient_N) {
    is_orient_N_ = is_orient_N;
    ComponentOrient orient = is_orient_N_ ? N : FS;
    double y_flip_axis = ly_ + height_ / 2.0;
    for (auto& component_ptr : components_) {
      double ly_to_axis = y_flip_axis - component_ptr->LLY();
      component_ptr->SetOrient(orient);
      component_ptr->SetURY(y_flip_axis + ly_to_axis);
    }
  }
}

void GriddedRow::InsertWellTapCell(Component& tap_cell, int loc) {
  tap_cell_ = &tap_cell;
  tap_cells_.push_back(&tap_cell);
  PlacePhysicalCell(tap_cell, loc);
}

void GriddedRow::PlacePhysicalCell(Component& cell, int loc) const {
  cell.SetCenterX(loc);
  Macro* macro_ptr = cell.MacroPtr();
  int p_well_height = macro_ptr->FirstPwellHeight();
  int n_well_height = macro_ptr->FirstNwellHeight();
  if (is_orient_N_) {
    cell.SetOrient(N);
    cell.SetLLY(ly_ + p_well_height_ - p_well_height);
  } else {
    cell.SetOrient(FS);
    cell.SetLLY(ly_ + n_well_height_ - n_well_height);
  }
}

Component* GriddedRow::WellTapCell() const { return tap_cell_; }

Component* GriddedRow::LeftWellTapCell() const {
  if (tap_cells_.empty()) {
    return nullptr;
  }
  return *std::min_element(tap_cells_.begin(), tap_cells_.end(),
                           [](const Component* lhs, const Component* rhs) {
                             return lhs->LLX() < rhs->LLX();
                           });
}

Component* GriddedRow::RightWellTapCell() const {
  if (tap_cells_.empty()) {
    return nullptr;
  }
  return *std::max_element(tap_cells_.begin(), tap_cells_.end(),
                           [](const Component* lhs, const Component* rhs) {
                             return lhs->URX() < rhs->URX();
                           });
}

void GriddedRow::UpdateComponentLocationCompact() {
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->LLX() < component_ptr1->LLX();
      });
  int current_x = lx_ + left_boundary_margin_;
  for (auto& component : components_) {
    component->SetLLX(current_x);
    component->SetCenterY(CenterY());
    current_x += component->Width();
  }
}

void GriddedRow::MinDisplacementLegalization() {
  std::sort(
      components_.begin(), components_.end(),
      [](const Component* component_ptr0, const Component* component_ptr1) {
        return component_ptr0->X() < component_ptr1->X();
      });

  std::vector<ComponentSegment> segments;

  DaliExpects(components_.size() == initial_locations_.size(),
              "Component number does not equal initial location number\n");

  size_t sz = components_.size();
  int lower_bound = lx_ + left_boundary_margin_;
  int upper_bound = lx_ + width_ - right_boundary_margin_;
  for (size_t i = 0; i < sz; ++i) {
    // create a segment which contains only this component
    Component* component_ptr = components_[i];
    double init_x = initial_locations_[component_ptr].x;
    if (init_x < lower_bound) {
      init_x = lower_bound;
    }
    if (init_x + component_ptr->Width() > upper_bound) {
      init_x = upper_bound - component_ptr->Width();
    }
    segments.emplace_back(component_ptr, init_x);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these
    // two segments repeats until this is no overlap or only one segment left

    ComponentSegment* cur_seg = &(segments[seg_sz - 1]);
    ComponentSegment* prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnLeft(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  // int count = 0;
  for (auto& seg : segments) {
    seg.UpdateComponentLocation();
    // count += seg.component_list.size();
    // seg.Report();
  }
}

void GriddedRow::UpdateMinDisplacementLLY() {
  DaliExpects(components_.size() == initial_locations_.size(),
              "Component count does not equal initial location count\n");
  double sum = 0;
  for (auto& [component_ptr, init_loc] : initial_locations_) {
    double init_np_boundary = init_loc.y;
    sum += init_np_boundary;
  }
  min_displacement_lly_ = sum / (int)(initial_locations_.size()) - PHeight();
}

double GriddedRow::MinDisplacementLLY() const { return min_displacement_lly_; }

std::vector<RowSegment>& GriddedRow::Segments() { return segments_; }

void GriddedRow::UpdateSegments(std::vector<SegI>& blockage,
                                bool is_existing_components_considered) {
  // collect used space segments
  std::vector<SegI> used_spaces = blockage;

  // treat existing components as blockages
  if (is_existing_components_considered) {
    for (auto& component_region : component_regions_) {
      Component* component = component_region.component;
      used_spaces.emplace_back(component->LLX(), component->URX());
    }
  }

  MergeIntervals(used_spaces);

  // collect unused space segments
  std::vector<int> intermediate_seg;
  if (used_spaces.empty()) {
    intermediate_seg.push_back(LLX());
    intermediate_seg.push_back(URX());
  } else {
    size_t segments_size = used_spaces.size();
    for (size_t i = 0; i < segments_size; ++i) {
      auto& interval = used_spaces[i];
      if (interval.lo == LLX() && interval.hi < URX()) {
        intermediate_seg.push_back(interval.hi);
      }

      if (interval.lo > LLX()) {
        if (intermediate_seg.empty()) {
          intermediate_seg.push_back(LLX());
        }
        intermediate_seg.push_back(interval.lo);
        if (interval.hi < URX()) {
          intermediate_seg.push_back(interval.hi);
        }
      }
    }
    if (intermediate_seg.size() % 2 == 1) {
      intermediate_seg.push_back(URX());
    }
  }

  // create sub-clusters
  size_t len = intermediate_seg.size();
  DaliExpects((len & 1) == 0, "odd number of segments? VERY ODD!");
  segments_.clear();
  segments_.reserve(len / 2);
  for (size_t i = 0; i < len; i += 2) {
    segments_.emplace_back();
    RowSegment& segment = segments_.back();
    segment.SetLLX(intermediate_seg[i]);
    segment.SetWidth(intermediate_seg[i + 1] - intermediate_seg[i]);
  }
}

/****
 * @brief: re-assign components to row segments
 */
void GriddedRow::AssignComponentsToSegments() {
  for (auto& [component, region_id] : component_regions_) {
    bool is_completely_in_a_seg = false;
    for (auto& seg : segments_) {
      if (component->LLX() >= seg.LLX() && component->URX() <= seg.URX()) {
        // double2d &init_loc = initial_locations_[component];
        seg.AddComponentRegion(component, region_id);
        is_completely_in_a_seg = true;
        break;
      }
    }
    DaliExpects(is_completely_in_a_seg,
                "A component is not completely in any row segment?!!");
  }
}

bool GriddedRow::IsBelowMiddleLine(Component* component) const {
  return component->LLY() < CenterY();
}

bool GriddedRow::IsBelowTopPlusKFirstRegionHeight(Component* component,
                                                  int iteration) const {
  return component->LLY() < URY() + height_ * (iteration - 1);
}

bool GriddedRow::IsAboveMiddleLine(Component* component) const {
  return component->URY() > CenterY();
}

bool GriddedRow::IsAboveBottomMinusKFirstRegionHeight(Component* component,
                                                      int iteration) const {
  return component->URY() > LLY() - height_ * (iteration - 1);
}

bool GriddedRow::IsOverlap(Component* component, int iteration,
                           bool is_upward) const {
  if (is_upward) {
    if (iteration > 0) {
      return IsBelowTopPlusKFirstRegionHeight(component, iteration);
    } else {
      return IsBelowMiddleLine(component);
    }
  } else {
    if (iteration > 0) {
      return IsAboveBottomMinusKFirstRegionHeight(component, iteration);
    } else {
      return IsAboveMiddleLine(component);
    }
  }
}

bool GriddedRow::IsOrientMatching(Component* component, int region_id) const {
  Macro* macro_ptr = component->MacroPtr();
  // cells with an odd number of regions can be fitted into any clusters
  if (macro_ptr->HasOddRegions()) {
    return true;
  }
  // cells with an even number of regions can only be fitted into clusters with
  // the same well orientation
  return IsOrientN() ? macro_ptr->IsNwellAbovePwell(region_id)
                     : !macro_ptr->IsNwellAbovePwell(region_id);
}

void GriddedRow::AddComponentRegion(Component* component, int region_id,
                                    bool is_upward) {
  component_regions_.emplace_back(component, region_id);
  Macro* macro_ptr = component->MacroPtr();
  int p_height = macro_ptr->PwellHeight(region_id, component->IsFlipped());
  int n_height = macro_ptr->NwellHeight(region_id, component->IsFlipped());
  if (is_upward) {
    UpdateWellHeightUpward(p_height, n_height);
  } else {
    UpdateWellHeightDownward(p_height, n_height);
  }
}

std::vector<ComponentRegion>& GriddedRow::ComponentRegions() {
  return component_regions_;
}

bool GriddedRow::AttemptToAdd(Component* component, bool is_upward) {
  // put this component to the closest white space segment
  double min_distance = DBL_MAX;
  int min_index = -1;
  int sz = static_cast<int>(segments_.size());
  for (int i = 0; i < sz; ++i) {
    auto& segment = segments_[i];
    double distance = DBL_MAX;
    if (segment.UsedSize() + component->Width() <= segment.Width()) {
      if (component->LLX() >= segment.LLX() &&
          component->URX() <= segment.URX()) {
        distance = 0;
      } else {
        distance = std::min(std::fabs(component->LLX() - segment.LLX()),
                            std::fabs(component->URX() - segment.URX()));
      }
    }
    if (distance < min_distance) {
      min_distance = distance;
      min_index = i;
    }
  }

  if (min_index == -1) {
    return false;
  }

  int region_count = component->MacroPtr()->RegionCount();
  int region_id = is_upward ? 0 : region_count - 1;
  segments_[min_index].AddComponentRegion(component, region_id);
  component->SetOrient(ComputeComponentOrient(component, is_upward));
  AddComponentRegion(component, region_id, is_upward);

  return true;
}

bool GriddedRow::AttemptToAddWithDispCheck(Component* component,
                                           double displacement_upper_limit,
                                           bool is_upward) {
  // put this component to the closest white space segment
  double min_distance = DBL_MAX;
  int min_index = -1;
  int sz = static_cast<int>(segments_.size());
  for (int i = 0; i < sz; ++i) {
    auto& segment = segments_[i];
    double distance = DBL_MAX;
    if (segment.UsedSize() + component->Width() <= segment.Width()) {
      if (component->LLX() >= segment.LLX() &&
          component->URX() <= segment.URX()) {
        distance = 0;
      } else {
        distance = std::min(std::fabs(component->LLX() - segment.LLX()),
                            std::fabs(component->URX() - segment.URX()));
      }
    }
    if (distance < min_distance && distance < displacement_upper_limit) {
      min_distance = distance;
      min_index = i;
    }
  }

  if (min_index == -1) {
    return false;
  }

  int region_count = component->MacroPtr()->RegionCount();
  int region_id = is_upward ? 0 : region_count - 1;
  segments_[min_index].AddComponentRegion(component, region_id);
  component->SetOrient(ComputeComponentOrient(component, is_upward));
  AddComponentRegion(component, region_id, is_upward);

  return true;
}

ComponentOrient GriddedRow::ComputeComponentOrient(Component* component,
                                                   bool is_upward) const {
  DaliExpects(component != nullptr, "Nullptr?");
  Macro* macro_ptr = component->MacroPtr();
  ComponentOrient orient = N;
  int region_count = macro_ptr->RegionCount();
  int region_id = is_upward ? 0 : region_count - 1;
  bool is_cluster_component_orientation_matching =
      (is_orient_N_ && macro_ptr->IsNwellAbovePwell(region_id)) ||
      (!is_orient_N_ && !macro_ptr->IsNwellAbovePwell(region_id));
  if (!is_cluster_component_orientation_matching) {
    orient = FS;
  }
  return orient;
}

void GriddedRow::LegalizeSegmentsX(bool use_init_loc) {
  for (auto& segment : segments_) {
    segment.MinDisplacementLegalization(use_init_loc);
    segment.SnapComponentsToPlacementGrid();
  }
}

void GriddedRow::LegalizeSegmentsY() {
  for (auto& [component, region_id] : component_regions_) {
    if (region_id != 0) continue;
    Macro* macro_ptr = component->MacroPtr();
    double y_loc = LLY();
    if (is_orient_N_) {
      y_loc +=
          p_well_height_ - macro_ptr->PwellHeight(0, component->IsFlipped());
    } else {
      y_loc +=
          n_well_height_ - macro_ptr->NwellHeight(0, component->IsFlipped());
    }
    component->SetLLY(y_loc);
  }
}

void GriddedRow::RecomputeHeight(int p_well_height, int n_well_height) {
  p_well_height_ = p_well_height;
  n_well_height_ = n_well_height;
  for (auto& component_region : component_regions_) {
    auto* component = component_region.component;
    int region_id = component_region.region_id;
    Macro* macro_ptr = component->MacroPtr();
    int p_height = macro_ptr->PwellHeight(region_id, component->IsFlipped());
    int n_height = macro_ptr->NwellHeight(region_id, component->IsFlipped());
    p_well_height_ = std::max(p_well_height_, p_height);
    n_well_height_ = std::max(n_well_height_, n_height);
  }
  height_ = p_well_height_ + n_well_height_;
}

void GriddedRow::InitializeComponentStretching() {
  for (auto& component_region : component_regions_) {
    auto* component = component_region.component;
    size_t region_id = component_region.region_id;
    Macro* macro_ptr = component->MacroPtr();
    size_t row_cnt = macro_ptr->RegionCount();
    if (region_id == 0) {
      component->StretchLengths().resize(row_cnt - 1, 0);
    }
  }
}

size_t GriddedRow::AddWellTapCells(Circuit* p_ckt, Macro* well_tap_macro,
                                   size_t start_id,
                                   std::vector<SegI>& well_tap_cell_locs) {
  double y_loc = LLY();
  if (is_orient_N_) {
    y_loc += p_well_height_ - well_tap_macro->PwellHeight(0, false);
  } else {
    y_loc += n_well_height_ - well_tap_macro->NwellHeight(0, true);
  }
  ComponentOrient orient = is_orient_N_ ? N : FS;
  for (auto& [lo_x, hi_x] : well_tap_cell_locs) {
    std::string component_name = "__well_tap__" + std::to_string(start_id++);
    auto [tap_cell, tap_cell_id] =
        p_ckt->design().WellTapComponentCollection().CreateWithId(
            component_name);
    tap_cell.SetPlacementStatus(PLACED);
    tap_cell.SetMacro(well_tap_macro);
    tap_cell.SetId(static_cast<int>(tap_cell_id));
    tap_cell.SetLLX(lo_x);
    tap_cell.SetLLY(y_loc);
    tap_cell.SetOrient(orient);
  }
  return start_id;
}

/****
 * @brief sort components in this row based on their x location
 *
 * If two cells have the same x location, then sort them based on their index.
 */
void GriddedRow::SortComponentRegions() {
  std::sort(component_regions_.begin(), component_regions_.end(),
            [](const ComponentRegion r0, const ComponentRegion r1) {
              return (r0.component->LLX() < r1.component->LLX()) ||
                     ((r0.component->LLX() == r1.component->LLX()) &&
                      (r0.component->Id() < r1.component->Id()));
            });
}

bool GriddedRow::IsRowLegal() {
  SortComponentRegions();
  int front = LLX();
  for (ComponentRegion& component_region : component_regions_) {
    Component* component_ptr = component_region.component;
    int component_lx = static_cast<int>(std::round(component_ptr->LLX()));
    if (component_lx < front) return false;
    front += component_ptr->Width();
  }
  return front <= URX();
}

size_t GriddedRow::CountComponentOverlaps() const {
  std::vector<Component*> components = components_;
  std::sort(components.begin(), components.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLX() < rhs->LLX()) ||
                     (lhs->LLX() == rhs->LLX() && lhs->Id() < rhs->Id());
            });

  size_t overlap_count = 0;
  for (size_t i = 0; i < components.size(); ++i) {
    const Component* lhs = components[i];
    for (size_t j = i + 1; j < components.size(); ++j) {
      const Component* rhs = components[j];
      if (rhs->LLX() >= lhs->URX()) {
        break;
      }
      bool overlaps_in_y = rhs->LLY() < lhs->URY() && rhs->URY() > lhs->LLY();
      if (overlaps_in_y) {
        ++overlap_count;
      }
    }
  }
  return overlap_count;
}

void GriddedRow::GenSubCellTable(std::ofstream& ost_cluster,
                                 std::ofstream& ost_sub_cell,
                                 std::ofstream& ost_discrepancy,
                                 std::ofstream& ost_displacement) {
  for (auto& seg : segments_) {
    seg.GenSubCellTable(ost_cluster, ost_sub_cell, ost_discrepancy,
                        ost_displacement, LLY(), URY());
  }
}

void GriddedRow::UpdateCommonSegment(std::vector<SegI>& avail_spaces, int width,
                                     double density) {
  std::vector<SegI> cur_spaces;
  for (auto& row_seg : segments_) {
    bool has_space = row_seg.Width() - row_seg.UsedSize() >= width;
    double tmp_density =
        static_cast<double>(row_seg.UsedSize() + width) / row_seg.Width();
    bool is_density_not_too_high = tmp_density < density * 1.1;
    if (has_space && is_density_not_too_high) {
      cur_spaces.emplace_back(row_seg.LLX(), row_seg.URX());
    }
  }

  std::vector<SegI> res;
  for (auto& avail_space : avail_spaces) {
    for (auto& cur_space : cur_spaces) {
      if (cur_space.lo >= avail_space.hi) break;
      SegI* joint_space = avail_space.Joint(cur_space);
      if (joint_space != nullptr && joint_space->Span() > 0) {
        res.emplace_back(joint_space->lo, joint_space->hi);
      }
      delete joint_space;
    }
  }

  avail_spaces = res;
}

void GriddedRow::AddStandardCell(Component* component, int region_id,
                                 SegI range) {
  bool is_added = false;
  for (auto& seg : segments_) {
    if ((seg.LLX() <= range.lo) && (seg.URX() >= range.hi)) {
      seg.AddComponentRegion(component, region_id);
      is_added = true;
      break;
    }
  }

  DaliExpects(is_added, "Unable to add component to a row segment?!");
}

size_t GriddedRow::OutOfBoundCell() {
  size_t cnt = 0;
  for (auto& component_region : component_regions_) {
    Component* component_ptr = component_region.component;
    if ((component_ptr->LLX() < LLX()) || (component_ptr->URX() > URX())) {
      ++cnt;
    }
  }
  return cnt;
}

void VerticalRowSegment::Merge(const VerticalRowSegment& next_segment,
                               int lower_bound, int upper_bound) {
  int sz = (int)next_segment.rows_.size();
  for (int i = 0; i < sz; ++i) {
    rows_.push_back(next_segment.rows_[i]);
  }
  height_ += next_segment.Height();

  sz = (int)rows_.size();
  int anchor_size = 0;
  for (auto* row : rows_) {
    anchor_size += (int)row->Components().size();
  }
  std::vector<double> anchor;
  anchor.reserve(anchor_size);
  int accumulative_d = 0;
  for (int i = 0; i < sz; ++i) {
    for (auto& [component_ptr, init_loc] : rows_[i]->InitLocations()) {
      double init_np_boundary = init_loc.y;
      anchor.push_back(init_np_boundary - accumulative_d);
    }
    accumulative_d += rows_[i]->NHeight();
    if (i + 1 < sz) {
      accumulative_d += rows_[i + 1]->PHeight();
    } else {
      accumulative_d += rows_[0]->PHeight();
    }
  }
  DaliExpects(height_ == accumulative_d,
              "Something is wrong, height does not match");

  long double sum = 0;
  for (auto& num : anchor) {
    sum += num;
  }
  int first_np_boundary = (int)std::round(sum / anchor_size);

  ly_ = first_np_boundary - rows_[0]->PHeight();
  if (ly_ < lower_bound) {
    ly_ = lower_bound;
  }
  if (ly_ + height_ > upper_bound) {
    ly_ = upper_bound - height_;
  }
}

void VerticalRowSegment::UpdateRowLocations() {
  int cur_y = ly_;
  int sz = (int)rows_.size();
  for (int i = 0; i < sz; ++i) {
    rows_[i]->SetLLY(cur_y);
    rows_[i]->UpdateComponentLocY();
    cur_y += rows_[i]->Height();
  }
}

}  // namespace dali
