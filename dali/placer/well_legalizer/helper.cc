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
      for (auto &cluster: stripe.gridded_rows_) {
        std::vector<int> llx;
        std::vector<int> lly;
        std::vector<int> urx;
        std::vector<int> ury;

        llx.push_back(cluster.LLX());
        lly.push_back(cluster.LLY());
        urx.push_back(cluster.URX());
        ury.push_back(cluster.URY());

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

void GenMATLABWellFillingTable(
    std::string const &base_file_name,
    std::vector<ClusterStripe> &col_list,
    int bottom_boundary,
    int top_boundary,
    int well_emit_mode
) {
  std::string p_file = base_file_name + "_pwell.txt";
  std::ofstream ostp(p_file.c_str());
  DaliExpects(ostp.is_open(), "Cannot open output file: " + p_file);

  std::string n_file = base_file_name + "_nwell.txt";
  std::ofstream ostn(n_file.c_str());
  DaliExpects(ostn.is_open(), "Cannot open output file: " + n_file);

  for (auto &col: col_list) {
    for (auto &stripe: col.stripe_list_) {
      std::vector<int> pn_edge_list;
      if (stripe.is_bottom_up_) {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
        pn_edge_list.push_back(bottom_boundary);
      } else {
        pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
        pn_edge_list.push_back(top_boundary);
      }
      for (auto &cluster: stripe.gridded_rows_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(top_boundary);
      } else {
        pn_edge_list.push_back(bottom_boundary);
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = stripe.is_first_row_orient_N_;
      int lx = stripe.LLX();
      int ux = stripe.URX();
      int ly;
      int uy;
      int rect_count = (int) pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (is_p_well_rect) {
          if (well_emit_mode != 1) {
            ostp << lx << "\t"
                 << ux << "\t"
                 << ux << "\t"
                 << lx << "\t"
                 << ly << "\t"
                 << ly << "\t"
                 << uy << "\t"
                 << uy << "\n";
          }
        } else {
          if (well_emit_mode != 2) {
            ostn << lx << "\t"
                 << ux << "\t"
                 << ux << "\t"
                 << lx << "\t"
                 << ly << "\t"
                 << ly << "\t"
                 << uy << "\t"
                 << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ostp.close();
  ostn.close();
}

}
