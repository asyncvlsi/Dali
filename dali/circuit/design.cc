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

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <iterator>

#include "dali/common/helper.h"

namespace dali {

static size_t FindFanoutBucket(const std::vector<size_t>& buckets,
                               size_t net_size) {
  auto it = std::lower_bound(buckets.begin(), buckets.end(), net_size);
  if (it == buckets.end()) {
    return buckets.size() - 1;
  }
  return static_cast<size_t>(std::distance(buckets.begin(), it));
}

static void NormalizeFanoutBuckets(std::vector<size_t>* buckets) {
  std::sort(buckets->begin(), buckets->end());
  buckets->erase(std::unique(buckets->begin(), buckets->end()), buckets->end());
}

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

void Design::AddFixedComponentPlacementBlockage(Component& component) {
  auto rect = ExpandOffGridPlacementBlockage(component.LLX(), component.LLY(),
                                             component.URX(), component.URY());
  fixed_component_blockages_.emplace_back(rect);
}

void Design::UpdateDieAreaPlacementBlockages() {
  die_area_dummy_blockages_.clear();
  for (auto& rect : die_area_.PlacementBlockages()) {
    die_area_dummy_blockages_.emplace_back(rect);
  }
}

void Design::UpdatePlacementBlockages() {
  all_blockages_.clear();
  all_blockages_.reserve(intrinsic_blockages_.size() +
                         fixed_component_blockages_.size() +
                         die_area_dummy_blockages_.size());

  for (auto& blockage : intrinsic_blockages_) {
    all_blockages_.push_back(blockage);
  }
  for (auto& blockage : fixed_component_blockages_) {
    all_blockages_.push_back(blockage);
  }
  for (auto& blockage : die_area_dummy_blockages_) {
    all_blockages_.push_back(blockage);
  }
}

const std::vector<PlacementBlockage>& Design::PlacementBlockages() const {
  return all_blockages_;
}

void Design::UpdateFanOutHistogram(size_t net_size) {
  if (net_histogram_.buckets.empty()) return;
  if (net_size <= 1) return;

  ++net_histogram_.counts[FindFanoutBucket(net_histogram_.buckets, net_size)];
}

void Design::InitNetFanOutHistogram(std::vector<size_t>* histo_x) {
  if (histo_x != nullptr) {
    net_histogram_.buckets = *histo_x;
    NormalizeFanoutBuckets(&net_histogram_.buckets);
  }

  size_t sz = net_histogram_.buckets.size();
  net_histogram_.counts.assign(sz, 0);
  net_histogram_.percents.assign(sz, 0);
  net_histogram_.sum_hpwls.assign(sz, 0);
  net_histogram_.ave_hpwls.assign(sz, 0);
  net_histogram_.min_hpwls.assign(sz, DBL_MAX);
  net_histogram_.max_hpwls.assign(sz, -DBL_MAX);
  net_histogram_.tot_net_count = 0;
  net_histogram_.tot_hpwl = 0;

  for (auto& net : nets_) {
    size_t net_size = net.PinCnt();
    UpdateFanOutHistogram(net_size);
  }

  for (size_t i = 0; i < sz; ++i) {
    net_histogram_.tot_net_count += net_histogram_.counts[i];
  }

  if (net_histogram_.tot_net_count == 0) return;

  for (size_t i = 0; i < sz; ++i) {
    net_histogram_.percents[i] =
        100.0 * static_cast<double>(net_histogram_.counts[i]) /
        static_cast<double>(net_histogram_.tot_net_count);
  }
}

void Design::UpdateNetHPWLHistogram(size_t net_size, double hpwl) {
  if (net_histogram_.buckets.empty()) return;
  if (net_size <= 1) return;

  size_t bucket_id = FindFanoutBucket(net_histogram_.buckets, net_size);

  net_histogram_.sum_hpwls[bucket_id] += hpwl;
  if (hpwl < net_histogram_.min_hpwls[bucket_id]) {
    net_histogram_.min_hpwls[bucket_id] = hpwl;
  }
  if (hpwl > net_histogram_.max_hpwls[bucket_id]) {
    net_histogram_.max_hpwls[bucket_id] = hpwl;
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

  LOG(info) << "\n";
  LOG(info) << "                                         Net histogram\n";
  LOG(info)
      << "====================================================================="
         "============================\n";
  LOG(info) << "  Net         Count     Percent/%      sum HPWL  "
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
    LOG(info) << buffer;
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
  LOG(info) << buffer;
  LOG(info)
      << "====================================================================="
         "============================\n";
  LOG(info) << " * HPWL unit, grid value in X: " << net_histogram_.hpwl_unit
            << " um\n";
  LOG(info) << "\n";
  // printf("%f\n", net_histogram_.tot_hpwl * 0.18);
}

}  // namespace dali
