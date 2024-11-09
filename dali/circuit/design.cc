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
#include "design.h"

#include <cfloat>
#include <cstdio>

#include "dali/common/helper.h"

namespace dali {

RectI Design::ExpandOffGridPlacementBlockage(double lx, double ly, double ux,
                                             double uy) {
  int new_lx = 0;
  if (AbsResidual(lx, 1) > 1e-5) {
    int shrunk_lx_ = static_cast<int>(std::round(std::floor(lx)));
    new_lx = shrunk_lx_;
  } else {
    new_lx = static_cast<int>(std::round(lx));
  }

  int new_ux = 0;
  if (AbsResidual(ux, 1) > 1e-5) {
    int shrunk_ux = static_cast<int>(std::round(std::ceil(ux)));
    new_ux = shrunk_ux;
  } else {
    new_ux = static_cast<int>(std::round(ux));
  }

  int new_ly = 0;
  if (AbsResidual(ly, 1) > 1e-5) {
    int shrunk_ly = static_cast<int>(std::round(std::floor(ly)));
    new_ly = shrunk_ly;
  } else {
    new_ly = static_cast<int>(std::round(ly));
  }

  int new_uy = 0;
  if (AbsResidual(uy, 1) > 1e-5) {
    int shrunk_uy = static_cast<int>(std::round(std::ceil(uy)));
    new_uy = shrunk_uy;
  } else {
    new_uy = static_cast<int>(std::round(uy));
  }

  return {new_lx, new_ly, new_ux, new_uy};
}

void Design::AddIntrinsicPlacementBlockage(double lx, double ly, double ux,
                                           double uy) {
  auto rect = ExpandOffGridPlacementBlockage(lx, ly, ux, uy);
  intrinsic_blockages_.emplace_back(rect);
}

void Design::AddFixedCellPlacementBlockage(Block &block) {
  auto rect = ExpandOffGridPlacementBlockage(block.LLX(), block.LLY(),
                                             block.URX(), block.URY());
  fixed_cell_blockages_.emplace_back(rect);
}

void Design::UpdateDieAreaPlacementBlockages() {
  for (auto &rect : die_area_.PlacementBlockages()) {
    die_area_dummy_blockages_.emplace_back(rect);
  }
}

void Design::UpdatePlacementBlockages() {
  all_blockages_.clear();
  all_blockages_.reserve(intrinsic_blockages_.size() +
                         fixed_cell_blockages_.size() +
                         die_area_dummy_blockages_.size());

  for (auto &blockage : intrinsic_blockages_) {
    all_blockages_.push_back(blockage);
  }
  for (auto &blockage : fixed_cell_blockages_) {
    all_blockages_.push_back(blockage);
  }
  for (auto &blockage : die_area_dummy_blockages_) {
    all_blockages_.push_back(blockage);
  }
}

const std::vector<PlacementBlockage> &Design::PlacementBlockages() const {
  return all_blockages_;
}

/****
 * Increment the count of net in the corresponding bin using binary search
 * ****/
void Design::UpdateFanOutHistogram(size_t net_size) {
  // if there is no net bins, skip
  if (net_histogram_.buckets.empty()) return;
  // if the net size is smaller than 2, skip
  if (net_size <= 1) return;

  // find the bin to increment the count
  size_t l = 0;
  size_t r = net_histogram_.buckets.size() - 1;
  while (l < r) {
    size_t m = l + (r - l) / 2;
    if (net_histogram_.buckets[m] == net_size) {
      ++net_histogram_.counts[m];
      return;
    }
    if (net_histogram_.buckets[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }
  ++net_histogram_.counts[l];
}

/****
 * Creates histogram bins using vector histo_x
 * If histo_x is a nullptr, then using default histogram bins
 * Classify nets to these bins, and compute the corresponding percentage
 * ****/
void Design::InitNetFanOutHistogram(std::vector<size_t> *histo_x) {
  if (histo_x != nullptr) {
    net_histogram_.buckets.clear();
    int sz = (int)histo_x->size();
    net_histogram_.buckets.assign(sz, 0);
    for (int i = 0; i < sz; ++i) {
      net_histogram_.buckets.push_back((*histo_x)[i]);
    }
  }

  size_t sz = net_histogram_.buckets.size();
  net_histogram_.counts.assign(sz, 0);
  net_histogram_.percents.assign(sz, 0);
  net_histogram_.sum_hpwls.assign(sz, 0);
  net_histogram_.ave_hpwls.assign(sz, 0);
  net_histogram_.min_hpwls.assign(sz, 0);
  net_histogram_.max_hpwls.assign(sz, 0);
  for (auto &net : nets_) {
    size_t net_size = net.PinCnt();
    UpdateFanOutHistogram(net_size);
  }

  net_histogram_.tot_net_count = 0;
  for (size_t i = 0; i < sz; ++i) {
    net_histogram_.tot_net_count += net_histogram_.counts[i];
  }
  for (size_t i = 0; i < sz; ++i) {
    net_histogram_.percents[i] =
        100.0 * static_cast<double>(net_histogram_.counts[i]) /
        static_cast<double>(net_histogram_.tot_net_count);
  }
}

/****
 * Increment the HPWL of a net in the corresponding bin using binary search
 * ****/
void Design::UpdateNetHPWLHistogram(size_t net_size, double hpwl) {
  // if there is no net bins, skip
  if (net_histogram_.buckets.empty()) return;
  // if the net size is smaller than 2, skip
  if (net_size <= 1) return;

  size_t l = 0;
  size_t r = net_histogram_.buckets.size() - 1;
  while (l < r) {
    size_t m = l + (r - l) / 2;
    if (net_histogram_.buckets[m] == net_size) {
      l = m;
      break;
    }
    if (net_histogram_.buckets[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }

  net_histogram_.sum_hpwls[l] += hpwl;
  if (hpwl < net_histogram_.min_hpwls[l]) {
    net_histogram_.min_hpwls[l] = hpwl;
  }
  if (hpwl > net_histogram_.max_hpwls[l]) {
    net_histogram_.max_hpwls[l] = hpwl;
  }
}

void Design::ReportNetFanOutHistogram() {
  if (net_histogram_.counts.empty()) return;
  size_t sz = net_histogram_.counts.size();
  for (size_t i = 0; i < sz; ++i) {
    if (net_histogram_.counts[i] > 0) {
      net_histogram_.ave_hpwls[i] =
          net_histogram_.sum_hpwls[i] /
          static_cast<double>(net_histogram_.counts[i]);
    } else {
      net_histogram_.ave_hpwls[i] = 0;
    }

    if (net_histogram_.min_hpwls[i] == DBL_MAX) {
      net_histogram_.min_hpwls[i] = 0;
    }
    if (net_histogram_.max_hpwls[i] == -DBL_MAX) {
      net_histogram_.max_hpwls[i] = 0;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
      << "                                         Net histogram\n";
  BOOST_LOG_TRIVIAL(info)
      << "====================================================================="
         "============================\n";
  BOOST_LOG_TRIVIAL(info) << "  Net         Count     Percent/%      sum HPWL  "
                             "      ave HPWL        min HPWL        max HPWL\n";
  size_t buffer_length = 1024;
  for (size_t i = 0; i < sz - 1; ++i) {
    size_t lo = net_histogram_.buckets[i];
    size_t hi = net_histogram_.buckets[i + 1] - 1;
    std::string buffer(buffer_length, '\0');
    int written_length;
    if (lo == hi) {
      written_length =
          snprintf(&buffer[0], buffer_length,
                   "%4ld       %8ld       %4.1f         %.2e        %.2e       "
                   " %.2e        %.2e\n",
                   lo, net_histogram_.counts[i], net_histogram_.percents[i],
                   net_histogram_.sum_hpwls[i], net_histogram_.ave_hpwls[i],
                   net_histogram_.min_hpwls[i], net_histogram_.max_hpwls[i]);
    } else {
      written_length =
          snprintf(&buffer[0], buffer_length,
                   "%4ld-%-4ld  %8ld       %4.1f         %.2e        %.2e      "
                   "  %.2e        %.2e\n",
                   lo, hi, net_histogram_.counts[i], net_histogram_.percents[i],
                   net_histogram_.sum_hpwls[i], net_histogram_.ave_hpwls[i],
                   net_histogram_.min_hpwls[i], net_histogram_.max_hpwls[i]);
    }
    buffer.resize(written_length);
    BOOST_LOG_TRIVIAL(info) << buffer;
  }
  std::string buffer(buffer_length, '\0');
  int written_length;
  written_length = snprintf(
      &buffer[0], buffer_length,
      "%4ld+      %8ld       %4.1f         %.2e        %.2e        %.2e        "
      "%.2e\n",
      net_histogram_.buckets[sz - 1], net_histogram_.counts[sz - 1],
      net_histogram_.percents[sz - 1], net_histogram_.sum_hpwls[sz - 1],
      net_histogram_.ave_hpwls[sz - 1], net_histogram_.min_hpwls[sz - 1],
      net_histogram_.max_hpwls[sz - 1]);
  buffer.resize(written_length);
  BOOST_LOG_TRIVIAL(info) << buffer;
  BOOST_LOG_TRIVIAL(info)
      << "====================================================================="
         "============================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                          << net_histogram_.hpwl_unit << " um\n";
  BOOST_LOG_TRIVIAL(info) << "\n";
  // printf("%f\n", net_histogram_.tot_hpwl * 0.18);
}

}  // namespace dali
