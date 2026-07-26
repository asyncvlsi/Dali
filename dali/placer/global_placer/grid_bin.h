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
#ifndef DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_
#define DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_

#include <cstddef>
#include <functional>
#include <ostream>
#include <set>
#include <vector>

#include "dali/circuit/component.h"
#include "dali/circuit/placement_blockage.h"
#include "dali/common/hash.h"

namespace dali {

/** Integer index of a grid bin in the look-ahead legalizer mesh. */
struct GridBinIndex {
 public:
  GridBinIndex() : x(0), y(0) {}
  GridBinIndex(int x0, int y0) : x(x0), y(y0) {}
  int x;
  int y;

  /** Reset the index to (0, 0). */
  void init() {
    x = 0;
    y = 0;
  };
  bool operator<(const GridBinIndex& rhs) const {
    bool is_less = (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
    return is_less;
  }
  bool operator>(const GridBinIndex& rhs) const {
    bool is_great = (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
    return is_great;
  }
  bool operator==(const GridBinIndex& rhs) const {
    return ((x == rhs.x) && (y == rhs.y));
  }
  friend std::ostream& operator<<(std::ostream& os, const GridBinIndex& p) {
    os << "(" << p.x << ", " << p.y << ") ";
    return os;
  }
};

struct GridBinIndexHasher {
  std::size_t operator()(const GridBinIndex& k) const {
    std::size_t seed = 0;
    HashCombine(seed, k.x);
    HashCombine(seed, k.y);
    return seed;
  }
};

/** Connected overfilled-bin cluster used by look-ahead legalization. */
struct OverfilledBinCluster {
 public:
  OverfilledBinCluster() : total_component_area(0), total_white_space(0) {}
  unsigned long long total_component_area;
  unsigned long long total_white_space;
  double capacity_demand = 0.0;
  double capacity = 0.0;
  double capacity_target_utilization = 1.0;
  std::set<GridBinIndex> bin_set;
  bool operator<(const OverfilledBinCluster& rhs) const {
    return (total_component_area < rhs.total_component_area);
  }
  bool operator>(const OverfilledBinCluster& rhs) const {
    return (total_component_area > rhs.total_component_area);
  }
  bool operator==(const OverfilledBinCluster& rhs) const {
    return (total_component_area == rhs.total_component_area);
  }
};

/** Mesh bin storing local component area, whitespace, blockages, and neighbors.
 */
class GridBin {
 public:
  GridBin();
  GridBinIndex index;
  int bottom;
  int top;
  int left;
  int right;
  unsigned long long white_space;
  unsigned long long component_area;
  double filling_rate;
  bool all_terminal;
  // a grid bin is over-filled, if filling rate is larger than the target, or
  // components locate on terminals
  bool over_fill;
  bool cluster_visited;
  bool global_placed;
  std::vector<Component*> component_ptrs;
  std::vector<const PlacementBlockage*> placement_blockages_;
  std::vector<GridBinIndex> adjacent_bin_index;

  /** Return left boundary in Dali grid units. */
  int LLX() const { return left; }

  /** Return bottom boundary in Dali grid units. */
  int LLY() const { return bottom; }

  /** Return right boundary in Dali grid units. */
  int URX() const { return right; }

  /** Return top boundary in Dali grid units. */
  int URY() const { return top; }

  /** Return bin height in Dali grid units. */
  int Height() const { return top - bottom; }

  /** Return bin width in Dali grid units. */
  int Width() const { return right - left; }

  /** Return bin area in grid-unit squared. */
  unsigned long long Area() const {
    return (unsigned long long)(top - bottom) *
           (unsigned long long)(right - left);
  }

  /** Return true when the bin is fully occupied by fixed components/blockages.
   */
  bool IsAllFixedComponent() const { return all_terminal; }

  /** Return true when the bin exceeds target utilization or has terminals. */
  bool OverFill() const { return over_fill; }

  /** Create the list of neighboring bin indices. */
  void create_adjacent_bin_list(int grid_cnt_x, int grid_cnt_y);

  /** Log grid-bin state for debugging. */
  void Report();
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_
