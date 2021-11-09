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

#ifndef DALI_DALI_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
#define DALI_DALI_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_

#include <iostream>
#include <set>

#include <boost/functional/hash.hpp>

namespace dali {

struct GridBinIndex {
 public:
  GridBinIndex() : x(0), y(0) {}
  GridBinIndex(int x0, int y0) : x(x0), y(y0) {}
  int x;
  int y;
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

}

#endif //DALI_DALI_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
