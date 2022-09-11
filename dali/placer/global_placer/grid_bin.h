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

#include <vector>

#include <boost/functional/hash.hpp>

#include "dali/circuit/block.h"

namespace dali {

struct GridBinIndex {
 public:
  GridBinIndex() : x(0), y(0) {}
  GridBinIndex(int32_t x0, int32_t y0) : x(x0), y(y0) {}
  int32_t x;
  int32_t y;
  void init() {
    x = 0;
    y = 0;
  };
  bool operator<(const GridBinIndex &rhs) const {
    bool is_less = (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
    return is_less;
  }
  bool operator>(const GridBinIndex &rhs) const {
    bool is_great = (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
    return is_great;
  }
  bool operator==(const GridBinIndex &rhs) const {
    return ((x == rhs.x) && (y == rhs.y));
  }
  friend std::ostream &operator<<(std::ostream &os, const GridBinIndex &p) {
    os << "(" << p.x << ", " << p.y << ") ";
    return os;
  }
};

struct GridBinIndexHasher {
  std::size_t operator()(const GridBinIndex &k) const {
    using boost::hash_value;
    using boost::hash_combine;

    // Start with a hash value of 0    .
    std::size_t seed = 0;

    // Modify 'seed' by XORing and bit-shifting in
    // one member of 'Key' after the other:
    hash_combine(seed, hash_value(k.x));
    hash_combine(seed, hash_value(k.y));

    // Return the result.
    return seed;
  }
};

struct GridBinCluster {
 public:
  GridBinCluster() : total_cell_area(0), total_white_space(0) {}
  unsigned long long total_cell_area;
  unsigned long long total_white_space;
  std::set<GridBinIndex> bin_set;
  bool operator<(const GridBinCluster &rhs) const {
    return (total_cell_area < rhs.total_cell_area);
  }
  bool operator>(const GridBinCluster &rhs) const {
    return (total_cell_area > rhs.total_cell_area);
  }
  bool operator==(const GridBinCluster &rhs) const {
    return (total_cell_area == rhs.total_cell_area);
  }
};

class GridBin {
 public:
  GridBin();
  GridBinIndex index;
  int32_t bottom;
  int32_t top;
  int32_t left;
  int32_t right;
  unsigned long long white_space;
  unsigned long long cell_area;
  double filling_rate;
  bool all_terminal;
  // a grid bin is over-filled, if filling rate is larger than the target, or cells locate on terminals
  bool over_fill;
  bool cluster_visited;
  bool global_placed;
  std::vector<Block *> cell_list;
  std::vector<Block *> fixed_blocks;
  std::vector<GridBinIndex> adjacent_bin_index;

  int32_t LLX() { return left; }
  int32_t LLY() { return bottom; }
  int32_t URX() { return right; }
  int32_t URY() { return top; }
  int32_t Height() { return top - bottom; }
  int32_t Width() { return right - left; }
  unsigned long long Area() {
    return (unsigned long long) (top - bottom)
        * (unsigned long long) (right - left);
  }
  bool IsAllFixedBlk() { return all_terminal; }
  bool OverFill() { return over_fill; }
  void create_adjacent_bin_list(int32_t grid_cnt_x, int32_t grid_cnt_y);
  void Report();
};

}

#endif //DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_
