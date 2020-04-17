//
// Created by Yihang Yang on 12/22/19.
//

#include "design.h"

void Design::UpdateNetFanoutHisto(int net_size) {
  /****
   * Increment the count of net in the corresponding bin using binary search
   * ****/

  if (net_size <= 1) return;
  int l = 0;
  int r = int(net_fanout_histo_x_.size()) - 1;

  while (l<r) {
    int m = l + (r - l) / 2;
    if (net_fanout_histo_x_[m] == net_size) {
      ++net_fanout_histo_y_[m];
      //std::cout << net_fanout_histo_x_[m] << " " << net_fanout_histo_x_[m+1] << " " << net_size << "\n";
      return;
    }
    if (net_fanout_histo_x_[m] > net_size) {
      r = m - 1;
    } else {
      l = m + 1;
    }
  }
  //std::cout << net_fanout_histo_x_[l] << " " << net_fanout_histo_x_[r] << " " << net_size << "\n";
  ++net_fanout_histo_y_[l];
}