/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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

#include "dali/placer/global_placer/look_ahead_spreader.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"

namespace dali {

LookAheadSpreader::LookAheadSpreader(
    Circuit* circuit,
    std::shared_ptr<const PlacementCapacityModel> capacity_model)
    : GlobalSpreader(circuit), capacity_model_(std::move(capacity_model)) {
  DaliExpects(capacity_model_ != nullptr,
              "Look-ahead spreader requires a capacity model");
}

/** Keep a component center inside a target leaf box.
 *
 * Look-ahead legalization moves centers rather than lower-left coordinates.
 * The valid center range is therefore shifted inward by half the component
 * size. If the target box is narrower than the component, use the box center as
 * the least surprising fallback and let later legalization handle exact row
 * legality.
 */
static double ClampCenterToBox(double center, double box_min, double box_max,
                               double component_size) {
  double half_size = component_size / 2.0;
  if (box_max - box_min <= component_size) {
    return (box_min + box_max) / 2.0;
  }
  return std::clamp(center, box_min + half_size, box_max - half_size);
}

/** Scale one coordinate of components into a leaf box.
 *
 * The packed center keeps local density under control by spreading components
 * according to their physical width/height. The affine center preserves the
 * relative geometry produced by the lower-bound quadratic solve. Blending the
 * two is a temporary compromise: it avoids the old full-repack HPWL damage
 * while still adding spreading pressure for dense leaf boxes.
 */
static void ScaleComponentCenters(
    std::vector<std::pair<Component*, double>>& locs, double box_min,
    double box_max, bool scale_x, double affine_scaling_weight) {
  if (locs.empty()) return;

  auto [min_it, max_it] = std::minmax_element(
      locs.begin(), locs.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
  double min_loc = min_it->second;
  double max_loc = max_it->second;
  double source_span = max_loc - min_loc;
  double target_span = box_max - box_min;

  std::sort(locs.begin(), locs.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.second < rhs.second;
  });
  double total_length = 0.0;
  for (auto& [component_ptr, loc] : locs) {
    (void)loc;
    total_length += scale_x ? component_ptr->Width() : component_ptr->Height();
  }
  if (total_length <= 1e-9) return;

  double cur_pos = 0.0;
  for (auto& [component_ptr, loc] : locs) {
    double component_size =
        scale_x ? component_ptr->Width() : component_ptr->Height();
    double packed_center =
        box_min + (cur_pos + component_size / 2.0) / total_length * target_span;
    double scaled_center = packed_center;
    if (source_span > 1e-9 && target_span > 1e-9) {
      double affine_center =
          box_min + (loc - min_loc) / source_span * target_span;
      scaled_center = affine_scaling_weight * affine_center +
                      (1.0 - affine_scaling_weight) * packed_center;
    }
    scaled_center =
        ClampCenterToBox(scaled_center, box_min, box_max, component_size);
    if (scale_x) {
      component_ptr->SetCenterX(scaled_center);
    } else {
      component_ptr->SetCenterY(scaled_center);
    }
    cur_pos += component_size;
  }
}

/****
 * @brief determine the grid bin height and width
 * grid_bin_height and grid_bin_width is determined by the following formula:
 *    grid_bin_height = sqrt(target_component_count_per_bin_ * average_area /
 * placement_density) the number of bins in the y-direction is given by:
 *    grid_cnt_y = (Top() - Bottom())/grid_bin_height
 *    grid_cnt_x = (Right() - Left())/grid_bin_width
 * And initialize the space of grid_bin_mesh
 */
void LookAheadSpreader::InitializeGridBinSize() {
  double grid_value_x = circuit_->GridValueX();
  double grid_value_y = circuit_->GridValueY();
  DaliExpects(grid_value_x > 0 && grid_value_y > 0,
              "Placement grid values must be positive");

  target_component_count_per_bin_ = TargetComponentCountPerBin();
  double grid_bin_area = target_component_count_per_bin_ *
                         circuit_->AverageMovableComponentArea() /
                         placement_density_;

  // Keep roughly the same bin area in Dali grid units, but make the bin close
  // to square in physical microns when x/y grid units have different sizes.
  double grid_y_to_x_ratio = grid_value_y / grid_value_x;
  grid_bin_height = static_cast<int>(
      std::round(std::sqrt(grid_bin_area / grid_y_to_x_ratio)));
  grid_bin_height = std::max(grid_bin_height, 1);
  grid_bin_width = std::max(
      1, static_cast<int>(std::round(grid_bin_height * grid_y_to_x_ratio)));
  grid_cnt_x =
      std::max(1, static_cast<int>(std::ceil(double(circuit_->RegionWidth()) /
                                             grid_bin_width)));
  grid_cnt_y =
      std::max(1, static_cast<int>(std::ceil(double(circuit_->RegionHeight()) /
                                             grid_bin_height)));
  LOG(debug) << "  Global placement bin width, height: " << grid_bin_width
             << "  " << grid_bin_height << "\n";
  LOG(debug) << "  Global placement bin physical width, height: "
             << grid_bin_width * grid_value_x << "  "
             << grid_bin_height * grid_value_y << "um\n";
  LOG(info) << "    LAL target components per bin: "
            << target_component_count_per_bin_ << "\n";

  std::vector<GridBin> temp_grid_bin_column(grid_cnt_y);
  grid_bin_mesh.resize(grid_cnt_x, temp_grid_bin_column);
}

/****
 * @brief set basic attributes for each grid bin.
 * we need to initialize many attributes in every single grid bin, including
 * index, boundaries, area, and potential available white space. The adjacent
 * bin list is cached for the convenience of overfilled bin clustering.
 */
void LookAheadSpreader::UpdateAttributesForAllGridBins() {
  for (int i = 0; i < grid_cnt_x; i++) {
    for (int j = 0; j < grid_cnt_y; j++) {
      grid_bin_mesh[i][j].index = {i, j};
      grid_bin_mesh[i][j].bottom = circuit_->RegionLLY() + j * grid_bin_height;
      grid_bin_mesh[i][j].top =
          circuit_->RegionLLY() + (j + 1) * grid_bin_height;
      grid_bin_mesh[i][j].left = circuit_->RegionLLX() + i * grid_bin_width;
      grid_bin_mesh[i][j].right =
          circuit_->RegionLLX() + (i + 1) * grid_bin_width;
      grid_bin_mesh[i][j].white_space = grid_bin_mesh[i][j].Area();
      // at the very beginning, assuming the white space is the same as area
      grid_bin_mesh[i][j].create_adjacent_bin_list(grid_cnt_x, grid_cnt_y);
    }
  }

  // make sure the top placement boundary is the same as the top of the topmost
  // bins
  for (int i = 0; i < grid_cnt_x; ++i) {
    grid_bin_mesh[i][grid_cnt_y - 1].top = circuit_->RegionURY();
    grid_bin_mesh[i][grid_cnt_y - 1].white_space =
        grid_bin_mesh[i][grid_cnt_y - 1].Area();
  }
  // make sure the right placement boundary is the same as the right of the
  // rightmost bins
  for (int i = 0; i < grid_cnt_y; ++i) {
    grid_bin_mesh[grid_cnt_x - 1][i].right = circuit_->RegionURX();
    grid_bin_mesh[grid_cnt_x - 1][i].white_space =
        grid_bin_mesh[grid_cnt_x - 1][i].Area();
  }
}

/****
 * @brief find fixed components in each grid bin
 * For each fixed component, we need to store its index in grid bins it overlaps
 * with. This can help us to compute available white space in each grid bin.
 */
void LookAheadSpreader::UpdatePlacementBlockagesInGridBins() {
  for (auto& blockage : circuit_->design().PlacementBlockages()) {
    const RectI& rect = blockage.GetRect();
    /* find the left, right, bottom, top index of the grid */
    bool blockage_component_out_of_region =
        rect.LLX() >= circuit_->RegionURX() ||
        rect.URX() <= circuit_->RegionLLX() ||
        rect.LLY() >= circuit_->RegionURY() ||
        rect.URY() <= circuit_->RegionLLY();
    // TODO: test and clean up this part of code on a design with placement
    // blockages that extend past the placement region
    if (blockage_component_out_of_region) continue;
    int left_index =
        std::floor((rect.LLX() - circuit_->RegionLLX()) / grid_bin_width);
    int right_index =
        std::floor((rect.URX() - circuit_->RegionLLX()) / grid_bin_width);
    int bottom_index =
        std::floor((rect.LLY() - circuit_->RegionLLY()) / grid_bin_height);
    int top_index =
        std::floor((rect.URY() - circuit_->RegionLLY()) / grid_bin_height);
    /* the grid boundaries might be the placement region boundaries
     * if a component touches the rightmost and topmost boundaries,
     * the index need to be fixed to make sure no memory access out of scope */
    if (left_index < 0) left_index = 0;
    if (right_index >= grid_cnt_x) right_index = grid_cnt_x - 1;
    if (bottom_index < 0) bottom_index = 0;
    if (top_index >= grid_cnt_y) top_index = grid_cnt_y - 1;

    /* for each terminal, we will check which grid is inside it, and directly
     * set the all_terminal attribute to true for that grid some small
     * terminals might occupy the same grid, we need to deduct the overlap
     * area from the white space of that grid bin when the final white space
     * is 0, we know this grid bin is occupied by several terminals*/
    for (int j = left_index; j <= right_index; ++j) {
      for (int k = bottom_index; k <= top_index; ++k) {
        /* the following case might happen:
         * the top/right of a fixed component overlap with the bottom/left of
         * a grid box. if this case happens, we need to ignore this fixed
         * component for this grid box. */
        bool component_out_of_bin = rect.LLX() >= grid_bin_mesh[j][k].right ||
                                    rect.URX() <= grid_bin_mesh[j][k].left ||
                                    rect.LLY() >= grid_bin_mesh[j][k].top ||
                                    rect.URY() <= grid_bin_mesh[j][k].bottom;
        if (component_out_of_bin) {
          continue;
        }
        grid_bin_mesh[j][k].placement_blockages_.push_back(&blockage);
      }
    }
  }
}

void LookAheadSpreader::UpdateWhiteSpaceInGridBin(GridBin& grid_bin) {
  RectI bin_rect(grid_bin.LLX(), grid_bin.LLY(), grid_bin.URX(),
                 grid_bin.URY());

  std::vector<RectI> rects;
  for (auto blockage_ptr : grid_bin.placement_blockages_) {
    auto& rect = blockage_ptr->GetRect();
    if (bin_rect.IsOverlap(rect)) {
      rects.push_back(bin_rect.GetOverlapRect(rect));
    }
  }

  unsigned long long used_area = GetCoverArea(rects);
  DaliExpects(grid_bin.white_space >= used_area,
              "Fixed components takes more space than available space? "
                  << grid_bin.white_space << " " << used_area);

  grid_bin.white_space -= used_area;
  if (grid_bin.white_space == 0) {
    grid_bin.all_terminal = true;
  }
}

/****
 * This function initialize the grid bin matrix, each bin has an area which
 * can accommodate around target_component_count_per_bin_ # of components
 * ****/
void LookAheadSpreader::InitGridBins() {
  grid_bin_mesh.clear();
  grid_bin_white_space_LUT.clear();
  InitializeGridBinSize();
  active_target_component_count_per_bin_ = target_component_count_per_bin_;
  UpdateAttributesForAllGridBins();
  UpdatePlacementBlockagesInGridBins();

  // update white spaces in grid bins
  for (auto& grid_bin_column : grid_bin_mesh) {
    for (auto& grid_bin : grid_bin_column) {
      UpdateWhiteSpaceInGridBin(grid_bin);
    }
  }
}

/**
 * Components per density bin for the current iteration, coarse to fine.
 *
 * A coarse grid gives early iterations a smoother spreading force; the finer
 * grid later exposes local congestion before legalization. `iteration_` is
 * supplied by GlobalPlacer::RunPlacementIterations via SetIteration() before
 * each Spread() call.
 */
int LookAheadSpreader::TargetComponentCountPerBin() const {
  if (iteration_ < 5) return 100;
  if (iteration_ < 15) return 60;
  return 30;
}

void LookAheadSpreader::RebuildGridBinsIfTargetChanged() {
  int target_component_count_per_bin = TargetComponentCountPerBin();
  if (target_component_count_per_bin ==
      active_target_component_count_per_bin_) {
    return;
  }
  InitGridBins();
  InitWhiteSpaceLUT();
}

/****
 * this is a member function to initialize white space look-up table
 * this table is a matrix, one way to calculate the white space in a region is
 * to add all white space of every single grid bin in this region an easier way
 * is to define an accumulate function and store it as a look-up table when we
 * want to find the white space in a region, the value can be easily extracted
 * from the look-up table
 * ****/
void LookAheadSpreader::InitWhiteSpaceLUT() {
  // this for loop is created to initialize the size of the loop-up table
  std::vector<unsigned long long> tmp_vector(grid_cnt_y);
  grid_bin_white_space_LUT.resize(grid_cnt_x, tmp_vector);

  // this for loop is used for computing elements in the look-up table
  // there are four cases, element at (0,0), elements on the left edge, elements
  // on the right edge, otherwise
  for (int kx = 0; kx < grid_cnt_x; ++kx) {
    for (int ky = 0; ky < grid_cnt_y; ++ky) {
      grid_bin_white_space_LUT[kx][ky] = 0;
      if (kx == 0) {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_mesh[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx][ky - 1] +
              grid_bin_mesh[kx][ky].white_space;
        }
      } else {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky] +
              grid_bin_mesh[kx][ky].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky] +
              grid_bin_white_space_LUT[kx][ky - 1] +
              grid_bin_mesh[kx][ky].white_space -
              grid_bin_white_space_LUT[kx - 1][ky - 1];
        }
      }
    }
  }
}

void LookAheadSpreader::Initialize(double placement_density) {
  placement_density_ = placement_density;

  upper_bound_hpwl_x_.clear();
  upper_bound_hpwl_y_.clear();
  upper_bound_hpwl_.clear();
  InitGridBins();
  InitWhiteSpaceLUT();
}

void LookAheadSpreader::ClearGridBinFlag() {
  for (auto& bin_column : grid_bin_mesh) {
    for (auto& bin : bin_column) bin.global_placed = false;
  }
}

/****
 * this is a member function to update grid bin status, because the
 * component_ptrs, component_area and over_fill state can be changed, so we need
 * to update them when necessary
 * ****/
void LookAheadSpreader::UpdateGridBinState() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // clean the old data
  for (auto& grid_bin_column : grid_bin_mesh) {
    for (auto& grid_bin : grid_bin_column) {
      grid_bin.component_ptrs.clear();
      grid_bin.component_area = 0;
      grid_bin.over_fill = false;
    }
  }

  // for each component, find the index of the grid bin it should be in.
  // note that in extreme cases, the index might be smaller than 0 or larger
  // than the maximum allowed index, because the component is on the boundaries,
  // so we need to make some modifications for these extreme cases.
  std::vector<Component>& components = circuit_->Components();
  int sz = static_cast<int>(components.size());
  int x_index = 0;
  int y_index = 0;

  for (int i = 0; i < sz; i++) {
    if (components[i].IsFixed()) continue;
    x_index = (int)std::floor((components[i].X() - circuit_->RegionLLX()) /
                              grid_bin_width);
    y_index = (int)std::floor((components[i].Y() - circuit_->RegionLLY()) /
                              grid_bin_height);
    if (x_index < 0) x_index = 0;
    if (x_index > grid_cnt_x - 1) x_index = grid_cnt_x - 1;
    if (y_index < 0) y_index = 0;
    if (y_index > grid_cnt_y - 1) y_index = grid_cnt_y - 1;
    grid_bin_mesh[x_index][y_index].component_ptrs.push_back(&(components[i]));
    grid_bin_mesh[x_index][y_index].component_area += components[i].Area();
  }

  /**** below is the criterion to decide whether a grid bin is over_filled or
   * not
   * 1. if this bin if fully occupied by fixed components, but its
   * component_ptrs is non-empty, which means there is some cells overlap with
   * this grid bin, we say it is over_fill
   * 2. if not fully occupied by fixed components, but filling_rate is larger
   * than the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with fixed
   *    components in this bin, we also mark it as over_fill
   * ****/
  // TODO: the third criterion might be changed in the next
  bool over_fill = false;
  last_overfilled_bin_count_ = 0;
  last_peak_bin_density_ = 0.0;
  for (auto& grid_bin_column : grid_bin_mesh) {
    for (auto& grid_bin : grid_bin_column) {
      if (grid_bin.global_placed) {
        grid_bin.over_fill = false;
        continue;
      }
      if (grid_bin.IsAllFixedComponent()) {
        if (!grid_bin.component_ptrs.empty()) {
          grid_bin.over_fill = true;
        }
      } else {
        PlacementCapacity capacity = capacity_model_->Evaluate(
            grid_bin.component_ptrs, grid_bin.Width(), grid_bin.Height(),
            grid_bin.white_space, placement_density_,
            CapacityEvaluationPurpose::kDensityBin);
        grid_bin.filling_rate = capacity.Utilization();
        if (capacity.IsOverfilled()) {
          grid_bin.over_fill = true;
        }
      }
      last_peak_bin_density_ =
          std::max(last_peak_bin_density_, grid_bin.filling_rate);
      if (!grid_bin.OverFill()) {
        for (auto& component_ptr : grid_bin.component_ptrs) {
          for (auto& blockage_ptr : grid_bin.placement_blockages_) {
            auto& rect = blockage_ptr->GetRect();
            over_fill = component_ptr->IsOverlap(rect);
            if (over_fill) {
              grid_bin.over_fill = true;
              break;
            }
          }
          if (over_fill) {
            break;
          }
          // two breaks have to be used to break two loops
        }
      }
      if (grid_bin.OverFill()) {
        ++last_overfilled_bin_count_;
      }
    }
  }
  elapsed_time.RecordEndTime();
  update_grid_bin_state_time_ += elapsed_time.GetWallTime();
}

void LookAheadSpreader::UpdateClusterArea(OverfilledBinCluster& cluster) {
  cluster.total_component_area = 0;
  cluster.total_white_space = 0;
  std::vector<Component*> components;
  GridBinIndex lower_left(grid_cnt_x - 1, grid_cnt_y - 1);
  GridBinIndex upper_right(0, 0);
  for (auto& index : cluster.bin_set) {
    const GridBin& bin = grid_bin_mesh[index.x][index.y];
    cluster.total_component_area += bin.component_area;
    cluster.total_white_space += bin.white_space;
    components.insert(components.end(), bin.component_ptrs.begin(),
                      bin.component_ptrs.end());
    lower_left.x = std::min(lower_left.x, index.x);
    lower_left.y = std::min(lower_left.y, index.y);
    upper_right.x = std::max(upper_right.x, index.x);
    upper_right.y = std::max(upper_right.y, index.y);
  }
  int width = grid_bin_mesh[upper_right.x][upper_right.y].right -
              grid_bin_mesh[lower_left.x][lower_left.y].left;
  int height = grid_bin_mesh[upper_right.x][upper_right.y].top -
               grid_bin_mesh[lower_left.x][lower_left.y].bottom;
  PlacementCapacity capacity = capacity_model_->Evaluate(
      components, width, height, cluster.total_white_space, placement_density_,
      CapacityEvaluationPurpose::kHotspot);
  cluster.capacity_demand = capacity.demand;
  cluster.capacity = capacity.capacity;
  cluster.capacity_target_utilization = capacity.target_utilization;
}

PlacementCapacity LookAheadSpreader::EvaluateWindow(
    const GridBinIndex& lower_left, const GridBinIndex& upper_right,
    unsigned long long whitespace_area,
    CapacityEvaluationPurpose purpose) const {
  std::vector<Component*> components;
  for (int x = lower_left.x; x <= upper_right.x; ++x) {
    for (int y = lower_left.y; y <= upper_right.y; ++y) {
      const auto& bin_components = grid_bin_mesh[x][y].component_ptrs;
      components.insert(components.end(), bin_components.begin(),
                        bin_components.end());
    }
  }
  int width = grid_bin_mesh[upper_right.x][upper_right.y].right -
              grid_bin_mesh[lower_left.x][lower_left.y].left;
  int height = grid_bin_mesh[upper_right.x][upper_right.y].top -
               grid_bin_mesh[lower_left.x][lower_left.y].bottom;
  return capacity_model_->Evaluate(components, width, height, whitespace_area,
                                   placement_density_, purpose);
}

void LookAheadSpreader::UpdateRegionCapacity(SpreadingRegion* region) const {
  DaliExpects(region != nullptr, "Cannot evaluate a null spreading region");
  region->total_white_space =
      LookUpWhiteSpace(region->ll_index, region->ur_index);
  PlacementCapacity capacity = EvaluateWindow(
      region->ll_index, region->ur_index, region->total_white_space,
      CapacityEvaluationPurpose::kSpreadingRegion);
  region->filling_rate = capacity.Utilization();
  region->capacity_target_utilization = capacity.target_utilization;
}

void LookAheadSpreader::UpdateClusterList() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  cluster_set.clear();

  int m = (int)grid_bin_mesh.size();     // number of rows
  int n = (int)grid_bin_mesh[0].size();  // number of columns
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) grid_bin_mesh[i][j].cluster_visited = false;
  }
  int cnt = 0;
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      if (grid_bin_mesh[i][j].cluster_visited || !grid_bin_mesh[i][j].over_fill)
        continue;
      GridBinIndex b(i, j);
      OverfilledBinCluster H;
      H.bin_set.insert(b);
      grid_bin_mesh[i][j].cluster_visited = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto& index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          GridBin& bin = grid_bin_mesh[index.x][index.y];
          if (!bin.cluster_visited && bin.over_fill) {
            if (cnt > cluster_upper_size) {
              UpdateClusterArea(H);
              cluster_set.insert(H);
              break;
            }
            bin.cluster_visited = true;
            H.bin_set.insert(index);
            ++cnt;
            Q.push(index);
          }
        }
      }
      UpdateClusterArea(H);
      cluster_set.insert(H);
    }
  }
  elapsed_time.RecordEndTime();
  update_cluster_list_time_ += elapsed_time.GetWallTime();
}

std::multiset<OverfilledBinCluster, std::greater<>>::iterator
LookAheadSpreader::SelectHotspotCluster() {
  if (cluster_set.empty()) return cluster_set.end();
  auto selected = cluster_set.begin();
  double selected_score = HotspotScore(*selected);
  for (auto it = std::next(cluster_set.begin()); it != cluster_set.end();
       ++it) {
    double score = HotspotScore(*it);
    if (score > selected_score) {
      selected = it;
      selected_score = score;
    }
  }
  return selected;
}

double LookAheadSpreader::HotspotScore(
    const OverfilledBinCluster& cluster) const {
  double component_area = static_cast<double>(cluster.total_component_area);
  double overflow = cluster.capacity_demand -
                    cluster.capacity_target_utilization * cluster.capacity;

  switch (hotspot_mode_) {
    case GlobalLalHotspotMode::kOverflow:
      return overflow;
    case GlobalLalHotspotMode::kOverflowRatio:
      if (cluster.capacity <= 0.0) {
        return std::numeric_limits<double>::infinity();
      }
      return cluster.capacity_demand / cluster.capacity -
             cluster.capacity_target_utilization;
    case GlobalLalHotspotMode::kComponentArea:
      return component_area;
  }
  return component_area;
}

const char* LookAheadSpreader::HotspotModeName(GlobalLalHotspotMode mode) {
  switch (mode) {
    case GlobalLalHotspotMode::kComponentArea:
      return "area";
    case GlobalLalHotspotMode::kOverflow:
      return "overflow";
    case GlobalLalHotspotMode::kOverflowRatio:
      return "overflow_ratio";
  }
  return "area";
}

void LookAheadSpreader::UpdateLargestCluster() {
  if (cluster_set.empty()) return;

  for (auto it = SelectHotspotCluster(); it != cluster_set.end();) {
    bool is_contact = true;

    // if there is no grid bin has been roughly legalized, then this cluster is
    // the selected hotspot for sure.
    for (auto& index : it->bin_set) {
      if (grid_bin_mesh[index.x][index.y].global_placed) {
        is_contact = false;
      }
    }
    if (is_contact) break;

    // initialize a list to store all indices in this cluster
    // initialize a map to store the visited flag during bfs
    std::vector<GridBinIndex> grid_bin_list;
    grid_bin_list.reserve(it->bin_set.size());
    std::unordered_map<GridBinIndex, bool, GridBinIndexHasher> grid_bin_visited;
    for (auto& index : it->bin_set) {
      grid_bin_list.push_back(index);
      grid_bin_visited.insert({index, false});
    }

    std::sort(grid_bin_list.begin(), grid_bin_list.end(),
              [](const GridBinIndex& index1, const GridBinIndex& index2) {
                return (index1.x < index2.x) ||
                       (index1.x == index2.x && index1.y < index2.y);
              });

    int cnt = 0;
    for (auto& grid_index : grid_bin_list) {
      int i = grid_index.x;
      int j = grid_index.y;
      if (grid_bin_visited[grid_index])
        continue;  // if this grid bin has been visited continue
      if (grid_bin_mesh[i][j].global_placed)
        continue;  // if this grid bin has been roughly legalized
      GridBinIndex b(i, j);
      OverfilledBinCluster H;
      H.bin_set.insert(b);
      grid_bin_visited[grid_index] = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto& index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          if (grid_bin_visited.find(index) == grid_bin_visited.end()) {
            continue;  // this index is not in the cluster
          }
          if (grid_bin_visited[index]) continue;  // this index has been visited
          if (grid_bin_mesh[index.x][index.y].global_placed)
            continue;  // if this grid bin has been roughly legalized
          if (cnt > cluster_upper_size) {
            UpdateClusterArea(H);
            cluster_set.insert(H);
            break;
          }
          grid_bin_visited[index] = true;
          H.bin_set.insert(index);
          ++cnt;
          Q.push(index);
        }
      }
      UpdateClusterArea(H);
      cluster_set.insert(H);
    }

    it = cluster_set.erase(it);
    it = SelectHotspotCluster();
  }
}

uint32_t LookAheadSpreader::LookUpWhiteSpace(
    GridBinIndex const& ll_index, GridBinIndex const& ur_index) const {
  /****
   * this function is used to return the white space in a region specified by
   * ll_index, and ur_index there are four cases, element at (0,0), elements on
   * the left edge, elements on the right edge, otherwise
   * ****/

  uint32_t total_white_space;
  /*if (ll_index.x == 0) {
  if (ll_index.y == 0) {
    total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
  } else {
    total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
        - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1];
  }
} else {
  if (ll_index.y == 0) {
    total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
        - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y];
  } else {
    total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
        - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1]
        - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y]
        + grid_bin_white_space_LUT[ll_index.x-1][ll_index.y-1];
  }
}*/
  GridBinWindow window = {ll_index.x, ll_index.y, ur_index.x, ur_index.y};
  total_white_space = LookUpWhiteSpace(window);
  return total_white_space;
}

uint32_t LookAheadSpreader::LookUpWhiteSpace(GridBinWindow& window) const {
  uint32_t total_white_space;
  if (window.llx == 0) {
    if (window.lly == 0) {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury];
    } else {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury] -
                          grid_bin_white_space_LUT[window.urx][window.lly - 1];
    }
  } else {
    if (window.lly == 0) {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury] -
                          grid_bin_white_space_LUT[window.llx - 1][window.ury];
    } else {
      total_white_space =
          grid_bin_white_space_LUT[window.urx][window.ury] -
          grid_bin_white_space_LUT[window.urx][window.lly - 1] -
          grid_bin_white_space_LUT[window.llx - 1][window.ury] +
          grid_bin_white_space_LUT[window.llx - 1][window.lly - 1];
    }
  }
  return total_white_space;
}

bool LookAheadSpreader::ExpandBoxByBestNeighbor(SpreadingRegion* box) {
  DaliExpects(box != nullptr, "Cannot expand a null LAL box");
  std::vector<SpreadingRegion> candidates;
  candidates.reserve(4);

  auto add_candidate = [&](int dlx, int dly, int durx, int dury) {
    SpreadingRegion candidate = *box;
    candidate.ll_index.x += dlx;
    candidate.ll_index.y += dly;
    candidate.ur_index.x += durx;
    candidate.ur_index.y += dury;
    candidate.UpdateComponentAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                                       grid_bin_mesh);
    UpdateRegionCapacity(&candidate);
    candidates.push_back(candidate);
  };

  if (box->ll_index.x > 0) add_candidate(-1, 0, 0, 0);
  if (box->ll_index.y > 0) add_candidate(0, -1, 0, 0);
  if (box->ur_index.x < grid_cnt_x - 1) add_candidate(0, 0, 1, 0);
  if (box->ur_index.y < grid_cnt_y - 1) add_candidate(0, 0, 0, 1);
  if (candidates.empty()) {
    return false;
  }

  auto score = [&](const SpreadingRegion& candidate) {
    double overflow = std::max(
        0.0, candidate.filling_rate - candidate.capacity_target_utilization);
    double area = double(candidate.ur_index.x - candidate.ll_index.x + 1) *
                  double(candidate.ur_index.y - candidate.ll_index.y + 1);
    double width = candidate.ur_index.x - candidate.ll_index.x + 1;
    double height = candidate.ur_index.y - candidate.ll_index.y + 1;
    double aspect_penalty = std::fabs(std::log(width / height));
    return overflow * 1e9 + area + aspect_penalty;
  };

  auto best_it = std::min_element(
      candidates.begin(), candidates.end(),
      [&](const SpreadingRegion& lhs, const SpreadingRegion& rhs) {
        return score(lhs) < score(rhs);
      });
  box->ll_index = best_it->ll_index;
  box->ur_index = best_it->ur_index;
  box->total_component_area = best_it->total_component_area;
  box->total_white_space = best_it->total_white_space;
  box->filling_rate = best_it->filling_rate;
  box->capacity_target_utilization = best_it->capacity_target_utilization;
  return true;
}

void LookAheadSpreader::FindMinimumBoxForLargestCluster() {
  /****
   * this function find the box for the largest cluster,
   * such that the total white space in the box is larger than the total
   * component area the way to do this is just by expanding the boundaries of
   * the bounding box of the first cluster
   *
   * Part 1
   * find the index of the maximum cluster
   * ****/
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // clear the spreading_region_queue_
  while (!spreading_region_queue_.empty()) spreading_region_queue_.pop();
  if (cluster_set.empty()) return;

  // Part 1
  SpreadingRegion R;
  R.cut_direction_x = false;

  R.ll_index.x = grid_cnt_x - 1;
  R.ll_index.y = grid_cnt_y - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  auto it = SelectHotspotCluster();
  last_hotspot_debug_ = HotspotDebugInfo();
  last_hotspot_debug_.bin_count = static_cast<int>(it->bin_set.size());
  last_hotspot_debug_.component_area = it->total_component_area;
  last_hotspot_debug_.white_space = it->total_white_space;
  last_hotspot_debug_.overflow =
      it->capacity_demand - it->capacity_target_utilization * it->capacity;
  last_hotspot_debug_.overflow_ratio =
      it->capacity == 0 ? 0
                        : it->capacity_demand / it->capacity -
                              it->capacity_target_utilization;
  last_hotspot_debug_.score = HotspotScore(*it);
  for (auto& index : it->bin_set) {
    R.ll_index.x = std::min(R.ll_index.x, index.x);
    R.ur_index.x = std::max(R.ur_index.x, index.x);
    R.ll_index.y = std::min(R.ll_index.y, index.y);
    R.ur_index.y = std::max(R.ur_index.y, index.y);
  }
  last_hotspot_debug_.cluster_ll = R.ll_index;
  last_hotspot_debug_.cluster_ur = R.ur_index;
  while (true) {
    // update component area, white space, and thus filling rate to determine
    // whether to expand this box or not
    R.UpdateComponentAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                               grid_bin_mesh);
    UpdateRegionCapacity(&R);
    if (R.filling_rate > R.capacity_target_utilization) {
      if (expansion_mode_ == GlobalLalExpansionMode::kBestNeighbor) {
        if (!ExpandBoxByBestNeighbor(&R)) {
          LOG(fatal) << "Reach maximum, cannot further expand\n";
        }
      } else {
        R.ExpandBox(grid_cnt_x, grid_cnt_y);
      }
    } else {
      break;
    }
    // LOG(info)   << R.total_white_space << "  " <<
    // R.filling_rate << "  " << FillingRate() << "\n";
  }

  R.UpdateComponentAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                             grid_bin_mesh);
  UpdateRegionCapacity(&R);
  last_hotspot_debug_.region_ll = R.ll_index;
  last_hotspot_debug_.region_ur = R.ur_index;
  last_hotspot_debug_.region_filling_rate = R.filling_rate;
  R.UpdateComponentList(grid_bin_mesh);
  R.ll_point.x = grid_bin_mesh[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_mesh[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_mesh[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_mesh[R.ur_index.x][R.ur_index.y].top;

  R.left = int(R.ll_point.x);
  R.bottom = int(R.ll_point.y);
  R.right = int(R.ur_point.x);
  R.top = int(R.ur_point.y);

  if (R.ll_index == R.ur_index) {
    R.UpdatePlacementBlockages(grid_bin_mesh);
    R.UpdateObsBoundary();
  } else {
    R.UpdatePlacementBlockages(grid_bin_mesh);
    R.UpdateObsBoundary();
  }
  spreading_region_queue_.push(R);
  ++last_hotspot_count_;
  last_max_hotspot_overflow_ =
      std::max(last_max_hotspot_overflow_, last_hotspot_debug_.overflow);
  LOG(debug) << "    LAL hotspot " << last_hotspot_count_ << " (mode "
             << HotspotModeName(hotspot_mode_) << "): bins "
             << last_hotspot_debug_.bin_count << ", component area "
             << last_hotspot_debug_.component_area << ", whitespace "
             << last_hotspot_debug_.white_space << ", overflow "
             << last_hotspot_debug_.overflow << ", overflow ratio "
             << last_hotspot_debug_.overflow_ratio << ", score "
             << last_hotspot_debug_.score << "\n"
             << "      cluster bins: " << last_hotspot_debug_.cluster_ll
             << "to " << last_hotspot_debug_.cluster_ur << "\n"
             << "      spreading region bins: " << last_hotspot_debug_.region_ll
             << "to " << last_hotspot_debug_.region_ur << ", filling rate "
             << last_hotspot_debug_.region_filling_rate << "\n";
  // LOG(info)   << "Bounding box total white space: " <<
  // spreading_region_queue_.front().total_white_space << "\n"; LOG(info) <<
  // "Bounding box total component area: " <<
  // spreading_region_queue_.front().total_component_area
  // << "\n";

  for (int kx = R.ll_index.x; kx <= R.ur_index.x; ++kx) {
    for (int ky = R.ll_index.y; ky <= R.ur_index.y; ++ky) {
      grid_bin_mesh[kx][ky].global_placed = true;
    }
  }

  elapsed_time.RecordEndTime();
  find_minimum_box_for_largest_cluster_time_ += elapsed_time.GetWallTime();
}

/****
 *
 *
 * @param box: region to partition into two child regions
 */
void LookAheadSpreader::SplitGridBox(SpreadingRegion& box) {
  // 1. create two sub-boxes
  SpreadingRegion box1, box2;
  // the first sub-box should have the same lower left corner as the original
  // box
  box1.left = box.left;
  box1.bottom = box.bottom;
  box1.ll_index = box.ll_index;
  box1.ur_index = box.ur_index;
  // the second sub-box should have the same upper right corner as the original
  // box
  box2.right = box.right;
  box2.top = box.top;
  box2.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;

  // 2. split along the direction with more boundary lines
  if (box.IsMoreHorizontalCutlines()) {
    box.cut_direction_x = true;
    // split the original box along the first horizontal cur line
    box1.right = box.right;
    box1.top = box.horizontal_cutlines[0];
    box2.left = box.left;
    box2.bottom = box.horizontal_cutlines[0];
    box1.UpdateWhiteSpaceAndFixedComponents(box.placement_blockages_);
    box2.UpdateWhiteSpaceAndFixedComponents(box.placement_blockages_);

    if (double(box1.total_white_space) / (double)box.total_white_space <=
        0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.component_ptrs = box.component_ptrs;
      box2.total_component_area = box.total_component_area;
      box2.UpdateObsBoundary();
      spreading_region_queue_.push(box2);
    } else if (double(box2.total_white_space) / (double)box.total_white_space <=
               0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.component_ptrs = box.component_ptrs;
      box1.total_component_area = box.total_component_area;
      box1.UpdateObsBoundary();
      spreading_region_queue_.push(box1);
    } else {
      box.UpdateCutPointComponentLists(box1.total_white_space,
                                       box2.total_white_space);
      box1.component_ptrs = box.component_ptrs_low;
      box2.component_ptrs = box.component_ptrs_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_component_area = box.total_component_area_low;
      box2.total_component_area = box.total_component_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      spreading_region_queue_.push(box1);
      spreading_region_queue_.push(box2);
    }
  } else {
    // box.Report();
    box.cut_direction_x = false;
    box1.right = box.vertical_cutlines[0];
    box1.top = box.top;
    box2.left = box.vertical_cutlines[0];
    box2.bottom = box.bottom;
    box1.UpdateWhiteSpaceAndFixedComponents(box.placement_blockages_);
    box2.UpdateWhiteSpaceAndFixedComponents(box.placement_blockages_);

    if (double(box1.total_white_space) / (double)box.total_white_space <=
        0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.component_ptrs = box.component_ptrs;
      box2.total_component_area = box.total_component_area;
      box2.UpdateObsBoundary();
      spreading_region_queue_.push(box2);
    } else if (double(box2.total_white_space) / (double)box.total_white_space <=
               0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.component_ptrs = box.component_ptrs;
      box1.total_component_area = box.total_component_area;
      box1.UpdateObsBoundary();
      spreading_region_queue_.push(box1);
    } else {
      box.UpdateCutPointComponentLists(box1.total_white_space,
                                       box2.total_white_space);
      box1.component_ptrs = box.component_ptrs_low;
      box2.component_ptrs = box.component_ptrs_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_component_area = box.total_component_area_low;
      box2.total_component_area = box.total_component_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      spreading_region_queue_.push(box1);
      spreading_region_queue_.push(box2);
    }
  }
}

void LookAheadSpreader::PlaceComponentInBox(SpreadingRegion& box) {
  int sz = static_cast<int>(box.component_ptrs.size());
  std::vector<std::pair<Component*, double>> index_loc_list_x(sz);
  std::vector<std::pair<Component*, double>> index_loc_list_y(sz);
  GridBin& grid_bin = grid_bin_mesh[box.ll_index.x][box.ll_index.y];
  for (int i = 0; i < sz; ++i) {
    Component* component_ptr = box.component_ptrs[i];
    index_loc_list_x[i].first = component_ptr;
    index_loc_list_x[i].second = component_ptr->X();
    index_loc_list_y[i].first = component_ptr;
    index_loc_list_y[i].second = component_ptr->Y();
    grid_bin.component_ptrs.push_back(component_ptr);
    grid_bin.component_area += component_ptr->Area();
  }

  // Preserve the lower-bound placement geometry when possible: spreading by
  // scaling local coordinates keeps the relative arrangement, whereas repacking
  // every leaf by sorted width/height discards the wirelength structure.
  ScaleComponentCenters(index_loc_list_x, box.left, box.right,
                        /*scale_x=*/true, affine_scaling_weight_);
  ScaleComponentCenters(index_loc_list_y, box.bottom, box.top,
                        /*scale_x=*/false, affine_scaling_weight_);
}

void LookAheadSpreader::SplitBox(SpreadingRegion& box) {
  bool flag_bisection_complete;
  int dominating_box_flag;  // indicate whether there is a dominating
                            // SpreadingRegion
  SpreadingRegion box1, box2;
  box1.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  // this part of code can be simplified, but after which the code might be
  // unclear cut-line along vertical direction
  if (box.cut_direction_x) {
    flag_bisection_complete = box.update_cut_index_white_space(
        grid_bin_white_space_LUT, grid_bin_mesh, macro_boundary_mode_);
    if (flag_bisection_complete) {
      box1.cut_direction_x = false;
      box2.cut_direction_x = false;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      // if bisection fail in one direction, do bisection in the other direction
      box.cut_direction_x = false;
      flag_bisection_complete = box.update_cut_index_white_space(
          grid_bin_white_space_LUT, grid_bin_mesh, macro_boundary_mode_);
      if (flag_bisection_complete) {
        box1.cut_direction_x = false;
        box2.cut_direction_x = false;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  } else {
    // cut-line along horizontal direction
    flag_bisection_complete = box.update_cut_index_white_space(
        grid_bin_white_space_LUT, grid_bin_mesh, macro_boundary_mode_);
    if (flag_bisection_complete) {
      box1.cut_direction_x = true;
      box2.cut_direction_x = true;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      box.cut_direction_x = true;
      flag_bisection_complete = box.update_cut_index_white_space(
          grid_bin_white_space_LUT, grid_bin_mesh, macro_boundary_mode_);
      if (flag_bisection_complete) {
        box1.cut_direction_x = true;
        box2.cut_direction_x = true;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  }
  box1.UpdateComponentAreaWhiteSpace(grid_bin_mesh);
  box2.UpdateComponentAreaWhiteSpace(grid_bin_mesh);
  // LOG(info)   << box1.ll_index_ << box1.ur_index_ << "\n";
  // LOG(info)   << box2.ll_index_ << box2.ur_index_ << "\n";
  // box1.update_all_terminal(grid_bin_matrix);
  // box2.update_all_terminal(grid_bin_matrix);
  //  if the white space in one bin is dominating the other, ignore the smaller
  //  one
  dominating_box_flag = 0;
  if (double(box1.total_white_space) / double(box.total_white_space) <= 0.01) {
    dominating_box_flag = 1;
  }
  if (double(box2.total_white_space) / double(box.total_white_space) <= 0.01) {
    dominating_box_flag = 2;
  }

  box1.UpdateBoundaries(grid_bin_mesh);
  box2.UpdateBoundaries(grid_bin_mesh);
  box1.UpdatePlacementBlockages(grid_bin_mesh);
  box1.UpdateObsBoundary();
  box2.UpdatePlacementBlockages(grid_bin_mesh);
  box2.UpdateObsBoundary();

  if (dominating_box_flag == 0) {
    // LOG(info)   << "component list size: " << box.component_ptrs.size()
    // << "\n"; box.UpdateComponentArea(component_list); LOG(info)   <<
    // "total_component_area: " << box.total_component_area << "\n";
    box.UpdateCutPointComponentLists(box1.total_white_space,
                                     box2.total_white_space);
    box1.component_ptrs = box.component_ptrs_low;
    box2.component_ptrs = box.component_ptrs_high;
    box1.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box1.ur_point = box.cut_ur_point;
    box2.ll_point = box.cut_ll_point;
    box1.total_component_area = box.total_component_area_low;
    box2.total_component_area = box.total_component_area_high;

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
  LOG(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM <<
"\n"; LOG(info)   << box1.left << " " << box1.bottom << "\n";
}
if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
  LOG(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM <<
"\n"; LOG(info)   << box2.left << " " << box2.bottom << "\n";
}*/

    spreading_region_queue_.push(box1);
    spreading_region_queue_.push(box2);
    // box1.write_box_boundary("first_bounding_box.txt", grid_bin_width,
    // grid_bin_height, LEFT, BOTTOM);
    // box2.write_box_boundary("first_bounding_box.txt", grid_bin_width,
    // grid_bin_height, LEFT, BOTTOM);
    // box1.WriteComponentRegion("first_cell_bounding_box.txt");
    // box2.WriteComponentRegion("first_cell_bounding_box.txt");
  } else if (dominating_box_flag == 1) {
    box2.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box2.component_ptrs = box.component_ptrs;
    box2.total_component_area = box.total_component_area;
    /*if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
  LOG(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM <<
"\n"; LOG(info)   << box2.left << " " << box2.bottom << "\n";
}*/

    spreading_region_queue_.push(box2);
    // box2.write_box_boundary("first_bounding_box.txt", grid_bin_width,
    // grid_bin_height, LEFT, BOTTOM);
    // box2.WriteComponentRegion("first_cell_bounding_box.txt");
  } else {
    box1.ll_point = box.ll_point;
    box1.ur_point = box.ur_point;
    box1.component_ptrs = box.component_ptrs;
    box1.total_component_area = box.total_component_area;
    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
  LOG(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM <<
"\n"; LOG(info)   << box1.left << " " << box1.bottom << "\n";
}*/

    spreading_region_queue_.push(box1);
    // box1.write_box_boundary("first_bounding_box.txt", grid_bin_width,
    // grid_bin_height, LEFT, BOTTOM);
    // box1.WriteComponentRegion("first_cell_bounding_box.txt");
  }
}

/****
 * keep splitting the biggest box to many small boxes, and keep update the shape
 * of each box and cells should be assigned to the box
 * @return true if succeed, false if fail
 */
bool LookAheadSpreader::RecursiveBisectionComponentSpreading() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  while (!spreading_region_queue_.empty()) {
    // std::cout << spreading_region_queue_.size() << "\n";
    if (spreading_region_queue_.empty()) break;
    SpreadingRegion& box = spreading_region_queue_.front();
    if (box.total_component_area == 0 || box.component_ptrs.empty()) {
      spreading_region_queue_.pop();
      continue;
    }
    // start moving cells to the box, if
    // (a) the box is a grid bin box or a smaller box
    // (b) and with no fixed macros inside
    if (box.ll_index == box.ur_index) {
      // UpdateGridBinComponents(box);
      if (box.HasPlacementBlockages()) {  // if there is a fixed macro inside a
                                          // box, keep splitting the box
        SplitGridBox(box);
        spreading_region_queue_.pop();
        continue;
      }
      /* if no terminals inside a box, do component placement inside the box */
      // PlaceComponentInBoxBisection(box);
      PlaceComponentInBox(box);
      // RoughLegalComponentInBox(box);
    } else {
      SplitBox(box);
    }
    spreading_region_queue_.pop();
  }

  elapsed_time.RecordEndTime();
  recursive_bisection_component_spreading_time_ += elapsed_time.GetWallTime();
  return true;
}

double LookAheadSpreader::Spread() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  last_hpwl_before_ = circuit_->WeightedHPWL();
  std::vector<double> lower_bound_center_x;
  std::vector<double> lower_bound_center_y;
  auto& components = circuit_->Components();
  lower_bound_center_x.reserve(components.size());
  lower_bound_center_y.reserve(components.size());
  for (const Component& component : components) {
    lower_bound_center_x.push_back(component.CenterX());
    lower_bound_center_y.push_back(component.CenterY());
  }

  RebuildGridBinsIfTargetChanged();
  ClearGridBinFlag();
  UpdateGridBinState();
  int overfilled_bin_count_before = last_overfilled_bin_count_;
  double peak_bin_density_before = last_peak_bin_density_;
  last_hotspot_count_ = 0;
  last_max_hotspot_overflow_ = 0.0;
  UpdateClusterList();
  do {
    UpdateLargestCluster();
    FindMinimumBoxForLargestCluster();
    RecursiveBisectionComponentSpreading();
    // LOG(info) << "cluster count: " << cluster_set.size() <<
    // "\n";
  } while (!cluster_set.empty());

  double evaluate_result_x = circuit_->WeightedHPWLX();
  upper_bound_hpwl_x_.push_back(evaluate_result_x);
  double evaluate_result_y = circuit_->WeightedHPWLY();
  upper_bound_hpwl_y_.push_back(evaluate_result_y);
  last_hpwl_after_ = evaluate_result_x + evaluate_result_y;
  ClearGridBinFlag();
  UpdateGridBinState();
  LOG(debug) << "Look-ahead legalization complete\n";

  double total_displacement_grid = 0.0;
  double max_displacement_grid = 0.0;
  double total_displacement_um = 0.0;
  double max_displacement_um = 0.0;
  int moved_component_count = 0;
  for (size_t i = 0; i < components.size(); ++i) {
    const Component& component = components[i];
    if (component.IsFixed()) continue;
    double dx_grid = component.CenterX() - lower_bound_center_x[i];
    double dy_grid = component.CenterY() - lower_bound_center_y[i];
    double displacement_grid = std::sqrt(dx_grid * dx_grid + dy_grid * dy_grid);
    double dx_um = dx_grid * circuit_->GridValueX();
    double dy_um = dy_grid * circuit_->GridValueY();
    double displacement_um = std::sqrt(dx_um * dx_um + dy_um * dy_um);
    total_displacement_grid += displacement_grid;
    max_displacement_grid = std::max(max_displacement_grid, displacement_grid);
    total_displacement_um += displacement_um;
    max_displacement_um = std::max(max_displacement_um, displacement_um);
    ++moved_component_count;
  }
  double avg_displacement_grid =
      moved_component_count == 0
          ? 0
          : total_displacement_grid /
                static_cast<double>(moved_component_count);
  double avg_displacement_um =
      moved_component_count == 0
          ? 0
          : total_displacement_um / static_cast<double>(moved_component_count);

  elapsed_time.RecordEndTime();
  tot_lal_time += elapsed_time.GetWallTime();


  LOG(debug) << "(UpdateGridBinState time: " << update_grid_bin_state_time_
             << "s)\n";
  LOG(debug) << "(UpdateClusterList time: " << update_cluster_list_time_
             << "s)\n";
  LOG(debug) << "(FindMinimumBoxForLargestCluster time: "
             << find_minimum_box_for_largest_cluster_time_ << "s)\n";
  LOG(debug) << "(RecursiveBisectionComponentSpreading time: "
             << recursive_bisection_component_spreading_time_ << "s)\n";

  LOG(info) << "    LAL density before/after: " << overfilled_bin_count_before
            << "/" << last_overfilled_bin_count_ << " bins over target, peak "
            << peak_bin_density_before << "/" << last_peak_bin_density_
            << ", hotspots: " << last_hotspot_count_
            << ", max hotspot overflow: " << last_max_hotspot_overflow_
            << ", HPWL delta: " << last_hpwl_after_ - last_hpwl_before_ << "\n";
  LOG(info) << "    LAL displacement avg/max: " << avg_displacement_grid << "/"
            << max_displacement_grid << " grid units, " << avg_displacement_um
            << "/" << max_displacement_um << " um\n";

  upper_bound_hpwl_.push_back(last_hpwl_after_);
  return upper_bound_hpwl_.back();
}

double LookAheadSpreader::GetTime() const { return tot_lal_time; }

void LookAheadSpreader::Close() {
  grid_bin_mesh.clear();
  grid_bin_white_space_LUT.clear();
}

}  // namespace dali
