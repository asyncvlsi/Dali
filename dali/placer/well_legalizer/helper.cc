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
#include "helper.h"

#include "dali/common/logging.h"

namespace dali {

void GenClusterTable(
    std::string const &name_of_file,
    std::vector<ClusterStripe> &col_list_
) {
  std::string cluster_file = name_of_file + "_cluster.txt";
  std::ofstream ost(cluster_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + cluster_file);

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.cluster_list_) {
        std::vector<int> llx;
        std::vector<int> lly;
        std::vector<int> urx;
        std::vector<int> ury;
        if (cluster.IsSingle()) {
          llx.push_back(cluster.LLX());
          lly.push_back(cluster.LLY());
          urx.push_back(cluster.URX());
          ury.push_back(cluster.URY());
        } else {
          llx.push_back(cluster.LLX());
          lly.push_back(cluster.LLY());
          urx.push_back(cluster.URX());
          ury.push_back(cluster.LLY() + cluster.ClusterEdge());

          llx.push_back(cluster.LLX());
          lly.push_back(cluster.LLY() + cluster.ClusterEdge());
          urx.push_back(cluster.URX());
          ury.push_back(cluster.URY());
        }
        size_t sz = llx.size();
        for (size_t i = 0; i < sz; ++i) {
          ost << llx[i] << "\t"
              << urx[i] << "\t"
              << urx[i] << "\t"
              << llx[i] << "\t"
              << lly[i] << "\t"
              << lly[i] << "\t"
              << ury[i] << "\t"
              << ury[i] << "\n";
        }
      }
    }
  }
  ost.close();
}

}
