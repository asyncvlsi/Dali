//
// Created by Yihang Yang on 12/22/19.
//

#include "design.h"

void Design::UpdateFanoutHisto(int net_size) {
  /****
   * Increment the count of net in the corresponding bin using binary search
   * ****/

  if (net_size <= 1) return;
  int l = 0;
  int r = int(net_histogram_.fanout_x_.size()) - 1;

  while (l < r) {
    int m = l + (r - l) / 2;
    if (net_histogram_.fanout_x_[m] == net_size) {
      ++net_histogram_.fanout_y_[m];
      //std::cout << fanout_x_[m] << " " << fanout_x_[m+1] << " " << net_size << "\n";
      return;
    }
    if (net_histogram_.fanout_x_[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }
  //std::cout << fanout_x_[l] << " " << fanout_x_[r] << " " << net_size << "\n";
  ++net_histogram_.fanout_y_[l];
}

void Design::InitNetFanoutHisto(std::vector<int> *histo_x) {
  /****
   * Creates histogram bins using vector histo_x
   * If histo_x is a nullptr, then using default histogram bins
   * Classify nets to these bins, and compute the corresponding percentage
   * ****/
  if (histo_x != nullptr) {
    net_histogram_.fanout_x_.clear();
    int sz = histo_x->size();
    net_histogram_.fanout_x_.assign(sz, 0);
    for (int i = 0; i < sz; ++i) {
      net_histogram_.fanout_x_.push_back((*histo_x)[i]);
    }
  }

  int sz = int(net_histogram_.fanout_x_.size());
  net_histogram_.fanout_y_.assign(sz, 0);
  net_histogram_.fanout_percent_.assign(sz, 0);
  net_histogram_.fanout_hpwl_.assign(sz, 0);
  net_histogram_.fanout_hpwl_per_pin_.assign(sz, 0);
  for (auto &net: net_list) {
    int net_size = net.P();
    UpdateFanoutHisto(net_size);
  }

  net_histogram_.tot_net_count_ = 0;
  for (int i = 0; i < sz; ++i) {
    net_histogram_.tot_net_count_ += net_histogram_.fanout_y_[i];
  }
  for (int i = 0; i < sz; ++i) {
    net_histogram_.fanout_percent_[i] = 100 * net_histogram_.fanout_y_[i] / (double) net_histogram_.tot_net_count_;
  }
}

void Design::UpdateNetHPWLHisto(int net_size, double hpwl) {
  /****
   * Increment the HPWL of a net in the corresponding bin using binary search
   * ****/

  if (net_size <= 1) return;
  int l = 0;
  int r = int(net_histogram_.fanout_x_.size()) - 1;

  while (l < r) {
    int m = l + (r - l) / 2;
    if (net_histogram_.fanout_x_[m] == net_size) {
      l = m;
      break;
    }
    if (net_histogram_.fanout_x_[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }

  net_histogram_.fanout_hpwl_[l] += hpwl;
  net_histogram_.fanout_hpwl_per_pin_[l] += hpwl / net_size;
}

void Design::ReportNetFanoutHisto() {
  int sz = net_histogram_.fanout_y_.size();
  printf("\n");
  printf("                           Net histogram\n");
  printf("===================================================================\n");
  printf("   Net         Count    Percent/%%      HPWL(um)      HPWL/netsize\n");
  for (int i = 0; i < sz - 1; ++i) {
    int lo = net_histogram_.fanout_x_[i];
    int hi = net_histogram_.fanout_x_[i + 1] - 1;
    if (lo == hi) {
      printf("%4d       %8d       %4.1f         %.2e        %.2e\n", lo,
             net_histogram_.fanout_y_[i],
             net_histogram_.fanout_percent_[i],
             net_histogram_.fanout_hpwl_[i],
             net_histogram_.fanout_hpwl_per_pin_[i]);
    } else {
      printf("%4d-%-4d  %8d       %4.1f         %.2e        %.2e\n", lo, hi,
             net_histogram_.fanout_y_[i],
             net_histogram_.fanout_percent_[i],
             net_histogram_.fanout_hpwl_[i],
             net_histogram_.fanout_hpwl_per_pin_[i]);
    }
  }
  printf("%4d+      %8d       %4.1f         %.2e        %.2e\n",
         net_histogram_.fanout_x_[sz - 1],
         net_histogram_.fanout_y_[sz - 1],
         net_histogram_.fanout_percent_[sz - 1],
         net_histogram_.fanout_hpwl_[sz - 1],
         net_histogram_.fanout_hpwl_per_pin_[sz - 1]);
  printf("===================================================================\n");
  printf("\n");
}
