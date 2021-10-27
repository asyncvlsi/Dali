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

namespace dali {

/****
 * Increment the count of net in the corresponding bin using binary search
 * ****/
void Design::UpdateFanoutHisto(int net_size) {
  if (net_size <= 1) return;
  int l = 0;
  int r = int(net_histogram_.bin_list_.size()) - 1;

  while (l < r) {
    int m = l + (r - l) / 2;
    if (net_histogram_.bin_list_[m] == net_size) {
      ++net_histogram_.count_[m];
      //BOOST_LOG_TRIVIAL(info)   << bin_list_[m] << " " << bin_list_[m+1] << " " << net_size << "\n";
      return;
    }
    if (net_histogram_.bin_list_[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }
  //BOOST_LOG_TRIVIAL(info)   << bin_list_[l] << " " << bin_list_[r] << " " << net_size << "\n";
  ++net_histogram_.count_[l];
}

/****
 * Creates histogram bins using vector histo_x
 * If histo_x is a nullptr, then using default histogram bins
 * Classify nets to these bins, and compute the corresponding percentage
 * ****/
void Design::InitNetFanoutHisto(std::vector<int> *histo_x) {
  if (histo_x != nullptr) {
    net_histogram_.bin_list_.clear();
    int sz = (int) histo_x->size();
    net_histogram_.bin_list_.assign(sz, 0);
    for (int i = 0; i < sz; ++i) {
      net_histogram_.bin_list_.push_back((*histo_x)[i]);
    }
  }

  int sz = int(net_histogram_.bin_list_.size());
  net_histogram_.count_.assign(sz, 0);
  net_histogram_.percent_.assign(sz, 0);
  net_histogram_.sum_hpwl_.assign(sz, 0);
  net_histogram_.ave_hpwl_.assign(sz, 0);
  net_histogram_.min_hpwl_.assign(sz, 0);
  net_histogram_.max_hpwl_.assign(sz, 0);
  for (auto &net: nets_) {
    int net_size = net.PinCnt();
    UpdateFanoutHisto(net_size);
  }

  net_histogram_.tot_net_count_ = 0;
  for (int i = 0; i < sz; ++i) {
    net_histogram_.tot_net_count_ += net_histogram_.count_[i];
  }
  for (int i = 0; i < sz; ++i) {
    net_histogram_.percent_[i] = 100 * net_histogram_.count_[i]
        / (double) net_histogram_.tot_net_count_;
  }
}

/****
 * Increment the HPWL of a net in the corresponding bin using binary search
 * ****/
void Design::UpdateNetHPWLHisto(int net_size, double hpwl) {
  if (net_size <= 1) return;
  int l = 0;
  int r = int(net_histogram_.bin_list_.size()) - 1;

  while (l < r) {
    int m = l + (r - l) / 2;
    if (net_histogram_.bin_list_[m] == net_size) {
      l = m;
      break;
    }
    if (net_histogram_.bin_list_[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }

  net_histogram_.sum_hpwl_[l] += hpwl;
  if (hpwl < net_histogram_.min_hpwl_[l]) {
    net_histogram_.min_hpwl_[l] = hpwl;
  }
  if (hpwl > net_histogram_.max_hpwl_[l]) {
    net_histogram_.max_hpwl_[l] = hpwl;
  }
}

void Design::ReportNetFanoutHisto() {
  int sz = static_cast<int>(net_histogram_.count_.size());
  for (int i = 0; i < sz; ++i) {
    if (net_histogram_.count_[i] > 0) {
      net_histogram_.ave_hpwl_[i] =
          net_histogram_.sum_hpwl_[i] / net_histogram_.count_[i];
    } else {
      net_histogram_.ave_hpwl_[i] = 0;
    }

    if (net_histogram_.min_hpwl_[i] == DBL_MAX) {
      net_histogram_.min_hpwl_[i] = 0;
    }
    if (net_histogram_.max_hpwl_[i] == DBL_MIN) {
      net_histogram_.max_hpwl_[i] = 0;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "                                         Net histogram\n";
  BOOST_LOG_TRIVIAL(info)
    << "=================================================================================================\n";
  BOOST_LOG_TRIVIAL(info)
    << "  Net         Count     Percent/%      sum HPWL        ave HPWL        min HPWL        max HPWL\n";

  for (int i = 0; i < sz - 1; ++i) {
    int lo = net_histogram_.bin_list_[i];
    int hi = net_histogram_.bin_list_[i + 1] - 1;
    std::string buffer(1024, '\0');
    int written_length = 0;
    if (lo == hi) {
      written_length =
          sprintf(&buffer[0],
                  "%4d       %8d       %4.1f         %.2e        %.2e        %.2e        %.2e\n",
                  lo,
                  net_histogram_.count_[i],
                  net_histogram_.percent_[i],
                  net_histogram_.sum_hpwl_[i],
                  net_histogram_.ave_hpwl_[i],
                  net_histogram_.min_hpwl_[i],
                  net_histogram_.max_hpwl_[i]);
    } else {
      written_length =
          sprintf(&buffer[0],
                  "%4d-%-4d  %8d       %4.1f         %.2e        %.2e        %.2e        %.2e\n",
                  lo,
                  hi,
                  net_histogram_.count_[i],
                  net_histogram_.percent_[i],
                  net_histogram_.sum_hpwl_[i],
                  net_histogram_.ave_hpwl_[i],
                  net_histogram_.min_hpwl_[i],
                  net_histogram_.max_hpwl_[i]);
    }
    buffer.resize(written_length);
    BOOST_LOG_TRIVIAL(info) << buffer;
  }
  std::string buffer(1024, '\0');
  int written_length = 0;
  written_length = sprintf(&buffer[0],
                           "%4d+      %8d       %4.1f         %.2e        %.2e        %.2e        %.2e\n",
                           net_histogram_.bin_list_[sz - 1],
                           net_histogram_.count_[sz - 1],
                           net_histogram_.percent_[sz - 1],
                           net_histogram_.sum_hpwl_[sz - 1],
                           net_histogram_.ave_hpwl_[sz - 1],
                           net_histogram_.min_hpwl_[sz - 1],
                           net_histogram_.max_hpwl_[sz - 1]);
  buffer.resize(written_length);
  BOOST_LOG_TRIVIAL(info) << buffer;

  BOOST_LOG_TRIVIAL(info)
    << "=================================================================================================\n";
  BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                          << net_histogram_.hpwl_unit_ << " um\n";
  BOOST_LOG_TRIVIAL(info) << "\n";
  //printf("%f\n", net_histogram_.tot_hpwl_ * 0.18);
}

}
