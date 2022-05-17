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
#include "rowsegment.h"

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/blocksegment.h"
#include "dali/placer/well_legalizer/optimizationhelper.h"
#include "dali/placer/well_legalizer/lgblkaux.h"

namespace dali {

void RowSegment::SetLLX(int lx) {
  lx_ = lx;
}

void RowSegment::SetURX(int ux) {
  lx_ = ux - width_;
}

void RowSegment::SetWidth(int width) {
  width_ = width;
}

void RowSegment::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

int RowSegment::LLX() const {
  return lx_;
}

int RowSegment::URX() const {
  return lx_ + width_;
}

int RowSegment::Width() const {
  return width_;
}

int RowSegment::UsedSize() const {
  return used_size_;
}

std::vector<BlockRegion> &RowSegment::BlkRegions() {
  return blk_regions_;
}

/****
 * Add a given block to this segment.
 * Update used space and the block list
 * @param blk_ptr: a pointer to the block which needs to be added to this segment
 */
void RowSegment::AddBlockRegion(Block *blk_ptr, int region_id) {
  used_size_ += blk_ptr->Width();
  blk_regions_.emplace_back(blk_ptr, region_id);
}

void RowSegment::MinDisplacementLegalization(bool use_init_loc) {
  if (blk_regions_.empty()) return;
  std::sort(
      blk_regions_.begin(),
      blk_regions_.end(),
      [](const BlockRegion &br0, const BlockRegion &br1) {
        return (br0.p_blk->LLX() < br1.p_blk->LLX()) ||
            ((br0.p_blk->LLX() == br1.p_blk->LLX())
                && (br0.p_blk->Id() < br1.p_blk->Id()));
      }
  );

  std::vector<BlkDispVar> vars;
  vars.reserve(blk_regions_.size());
  if (use_init_loc) {
    for (auto &blk_rgn: blk_regions_) {
      auto aux_ptr = static_cast<LgBlkAux *>(blk_rgn.p_blk->AuxPtr());
      vars.emplace_back(blk_rgn.p_blk->Width(), aux_ptr->InitLoc().x, 1.0);
      vars.back().blk_rgn = blk_rgn;
    }
  } else {
    for (auto &blk_rgn: blk_regions_) {
      vars.emplace_back(blk_rgn.p_blk->Width(), blk_rgn.p_blk->LLX(), 1.0);
      vars.back().blk_rgn = blk_rgn;
    }
  }

  // get optimized locations and store them in vars
  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  // set block locations from vars
  for (auto &var: vars) {
    var.UpdateBlkLocation();
  }
}

void RowSegment::SnapCellToPlacementGrid() {
  for (auto &blk_region: blk_regions_) {
    blk_region.p_blk->SetLLX(std::round(blk_region.p_blk->LLX()));
  }
}

void RowSegment::SetOptimalAnchorWeight(double weight) {
  opt_anchor_weight_ = weight;
}

void RowSegment::FitInRange(std::vector<BlkDispVar> &vars) {
  double left_contour = LLX();
  for (auto &var: vars) {
    if (var.Solution() < left_contour) {
      var.SetSolution(left_contour);
    }
    left_contour = var.Solution() + var.Width();
  }

  double right_contour = URX();
  for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
    auto &var = *it;
    int width = var.Width();
    double ux = var.Solution() + width;
    if (ux > right_contour) {
      var.SetSolution(right_contour - width);
    }
    right_contour = var.Solution();
  }
}

double RowSegment::DispCost(
    std::vector<BlkDispVar> &vars,
    int l,
    int r,
    bool is_linear
) {
  double quadratic_disp = 0;
  for (int i = l; i <= r; ++i) {
    double disp = std::fabs(vars[i].Solution() - vars[i].InitX());
    if (is_linear) {
      quadratic_disp += vars[i].Weight();
    } else {
      quadratic_disp += vars[i].Weight() * disp * disp;
    }

  }
  return quadratic_disp;
}

void RowSegment::FindBestLocalOrder(
    std::vector<BlkDispVar> &res,
    double &cost,
    std::vector<BlkDispVar> &vars,
    int cur, int l, int r,
    double left_bound, double right_bound,
    double gap, int range,
    bool is_linear
) {
  if (cur == r) {
    vars[l].SetSolution(left_bound);
    vars[r].SetSolution(right_bound - vars[r].Width());

    double left_contour = left_bound + gap + vars[l].Width();
    for (int i = l + 1; i < r; ++i) {
      vars[i].SetSolution(left_contour);
      left_contour += vars[i].Width() + gap;
    }

    double tmp_cost = DispCost(vars, l, r, is_linear);
    if (tmp_cost < cost) {
      cost = tmp_cost;
      for (int j = 0; j < range; ++j) {
        res[j] = vars[l + j];
      }
    }
  } else {
    // Permutations made
    for (int i = cur; i <= r; ++i) {
      // Swapping done
      std::swap(vars[cur], vars[i]);

      // Recursion called
      FindBestLocalOrder(
          res, cost, vars,
          cur + 1, l, r,
          left_bound, right_bound, gap, range,
          is_linear
      );

      //backtrack
      std::swap(vars[cur], vars[i]);
    }
  }
}

void RowSegment::LocalReorder(
    std::vector<BlkDispVar> &vars,
    int range,
    int omit,
    bool is_linear
) {
  int sz = static_cast<int>(vars.size());
  if (sz < range) return;

  int last_segment = sz - range - omit;
  BlkDispVar tmp(0, 0, 0);
  std::vector<BlkDispVar> res_local_order(range, tmp);
  for (int l = omit; l <= last_segment; ++l) {
    int tot_blk_width = 0;
    for (int j = 0; j < range; ++j) {
      res_local_order[j] = vars[l + j];
      tot_blk_width += res_local_order[j].Width();
    }
    int r = l + range - 1;
    double best_cost = DBL_MAX;
    double left_bound = vars[l].Solution();
    double right_bound = vars[r].Solution() + vars[r].Width();
    double gap = (right_bound - left_bound - tot_blk_width) / (range - 1);

    FindBestLocalOrder(
        res_local_order, best_cost, vars,
        l, l, r,
        left_bound, right_bound, gap, range,
        is_linear
    );
    for (int j = 0; j < range; ++j) {
      vars[l + j] = res_local_order[j];
    }

    vars[l].SetSolution(left_bound);
    vars[r].SetSolution(right_bound - vars[r].Width());
    double left_contour = left_bound + vars[l].Width() + gap;
    for (int i = l + 1; i < r; ++i) {
      vars[i].SetSolution(left_contour);
      left_contour += vars[i].Width() + gap;
    }
  }
}

void RowSegment::LocalReorder2(
    std::vector<BlkDispVar> &vars
) {
  int sz = static_cast<int>(vars.size());
  if (sz <= 2) return;
  for (int i = 0; i < sz - 1;) {
    // if a multideck cell
    if (vars[i].IsMultideckCell()) {
      if (vars[i].TendToRight()) { // have a tendency to move right
        if (i + 1 < sz) {
          if ((vars[i + 1].IsMultideckCell() && vars[i + 1].TendToLeft()) ||
              (!vars[i + 1].IsMultideckCell())) {
            double left = vars[i].Solution();
            double right = vars[i + 1].Solution() + vars[i + 1].Width();
            vars[i].SetSolution(right - vars[i].Width());
            vars[i + 1].SetSolution(left);
            i += 1;
            continue;
          }
        }
      } else if (vars[i].TendToLeft()) { // have a tendency to move left
        if (i - 1 > 0) {
          if ((vars[i - 1].IsMultideckCell() && vars[i - 1].TendToRight()) ||
              (!vars[i - 1].IsMultideckCell())) {
            double left = vars[i - 1].Solution();
            double right = vars[i].Solution() + vars[i].Width();
            vars[i - 1].SetSolution(right - vars[i - 1].Width());
            vars[i].SetSolution(left);
            i += 1;
            continue;
          }
        }
      }
    }
    ++i;
  }
}

std::vector<BlkDispVar> RowSegment::OptimizeQuadraticDisplacement(
    double lambda,
    bool is_weighted_anchor,
    bool is_reorder
) {
  std::vector<BlkDispVar> vars;
  if (blk_regions_.empty()) return vars;

  // sort cells based on their lower x location
  std::sort(
      blk_regions_.begin(),
      blk_regions_.end(),
      [](const BlockRegion &br0, const BlockRegion &br1) {
        return (br0.p_blk->LLX() < br1.p_blk->LLX()) ||
            ((br0.p_blk->LLX() == br1.p_blk->LLX())
                && (br0.p_blk->Id() < br1.p_blk->Id()));
      }
  );

  // compute average discrepancy
  double ave_discrepancy = 1;
  if (is_weighted_anchor) {
    int sub_cell_cnt = 0;
    double sum_discrepancy = 0;
    for (auto &blk_rgn: blk_regions_) {
      Block *blk_ptr = blk_rgn.p_blk;
      auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
      double average_loc = aux_ptr->AverageLoc();
      double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      sum_discrepancy += tmp_discrepancy;
      ++sub_cell_cnt;
    }
    ave_discrepancy = sum_discrepancy / sub_cell_cnt;
    if (ave_discrepancy < 1e-5) {
      ave_discrepancy = 1e-5;
    }
  }

  //vars.reserve(blk_regions_.size() + 2);
  //vars.emplace_back(0, LLX(), 0);
  //double max_weight = 0;
  vars.reserve(blk_regions_.size());
  for (auto &blk_rgn: blk_regions_) {
    Block *blk_ptr = blk_rgn.p_blk;
    int region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    vars.emplace_back(
        blk_ptr->Width(),
        aux_ptr->InitLoc().x,
        lambda / region_cnt
    );
    vars.back().blk_rgn = blk_rgn;
    if (region_cnt <= 1) continue;
    double weight_discrepancy = 1;
    if (is_weighted_anchor) {
      double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
      double average_loc = aux_ptr->AverageLoc();
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      weight_discrepancy = pow(1 + tmp_discrepancy / ave_discrepancy, 2.0);
      //weight_discrepancy = exp(tmp_discrepancy / ave_discrepancy);
    }
    double weight = (1 - lambda) * weight_discrepancy;
    //max_weight = std::max(weight, max_weight);
    vars.back().SetAnchor(
        aux_ptr->AverageLoc(),
        weight // / region_cnt
    );
  }
  //vars.emplace_back(0, URX(), max_weight * 100);
  //vars[0].SetWeight(max_weight * 100);

  //MinimizeQuadraticDisplacement(vars, LLX(), URX());
  //MinimizeQuadraticDisplacement(vars);
  AbacusPlaceRow(vars);

  if (is_weighted_anchor) {
    FitInRange(vars);
    if (is_reorder) {
      LocalReorder(vars, 3, 0, false);
      //LocalReorder2(vars);
    }
  }

  return vars;
}

std::vector<BlkDispVar> RowSegment::OptimizeLinearDisplacement(
    double lambda,
    bool is_weighted_anchor,
    bool is_reorder
) {
  std::vector<BlkDispVar> vars;
  if (blk_regions_.empty()) return vars;

  // sort cells based on their lower x location
  std::sort(
      blk_regions_.begin(),
      blk_regions_.end(),
      [](const BlockRegion &br0, const BlockRegion &br1) {
        return (br0.p_blk->LLX() < br1.p_blk->LLX()) ||
            ((br0.p_blk->LLX() == br1.p_blk->LLX())
                && (br0.p_blk->Id() < br1.p_blk->Id()));
      }
  );

  // compute average discrepancy
  double ave_discrepancy = 1;
  if (is_weighted_anchor) {
    int sub_cell_cnt = 0;
    double sum_discrepancy = 0;
    for (auto &blk_rgn: blk_regions_) {
      Block *blk_ptr = blk_rgn.p_blk;
      auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
      //int region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
      double average_loc = aux_ptr->AverageLoc();
      double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      sum_discrepancy += tmp_discrepancy;
      ++sub_cell_cnt;
    }
    ave_discrepancy = sum_discrepancy / sub_cell_cnt;
    if (ave_discrepancy < 1e-5) {
      ave_discrepancy = 1e-5;
    }
  }

  // create variables
  vars.reserve(blk_regions_.size());
  for (auto &blk_rgn: blk_regions_) {
    Block *blk_ptr = blk_rgn.p_blk;
    int region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    vars.emplace_back(
        blk_ptr->Width(),
        aux_ptr->InitLoc().x,
        lambda / region_cnt
    );
    vars.back().blk_rgn = blk_rgn;
    if (region_cnt <= 1) continue;
    double weight_discrepancy = 1;
    if (is_weighted_anchor) {
      double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
      double average_loc = aux_ptr->AverageLoc();
      double tmp_discrepancy = std::fabs(average_loc - sub_loc);
      weight_discrepancy = pow(1 + tmp_discrepancy / ave_discrepancy, 1.0);
    }
    vars.back().SetAnchor(
        aux_ptr->AverageLoc(),
        (1 - lambda) * weight_discrepancy // / region_cnt
    );
  }

  //MinimizeLinearDisplacement(vars, LLX(), URX());
  MinimizeLinearDisplacement(vars);

  if (is_weighted_anchor) {
    FitInRange(vars);
    if (is_reorder) LocalReorder(vars, 3, 0, true);
  }

  return vars;
}

void RowSegment::GenSubCellTable(
    std::ofstream &ost_cluster,
    std::ofstream &ost_sub_cell,
    std::ofstream &ost_discrepancy,
    std::ofstream &ost_displacement,
    double row_ly,
    double row_uy
) {
  SaveMatlabPatchRect(
      ost_cluster,
      static_cast<double>(LLX()), row_ly,
      static_cast<double>(URX()), row_uy,
      false, 0, 0, 0
  );

  for (auto &blk_rgn: blk_regions_) {
    Block *blk_ptr = blk_rgn.p_blk;
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    double ly = std::max(blk_ptr->LLY(), row_ly);
    double uy = std::min(blk_ptr->URY(), row_uy);
    double sub_x = aux_ptr->SubLocs()[blk_rgn.region_id];
    double sub_y = ly;
    SaveMatlabPatchRect(
        ost_sub_cell,
        sub_x, ly, sub_x + blk_ptr->Width(), uy,
        true, 0, 1, 1
    );

    double disc_x = aux_ptr->AverageLoc() - sub_x;
    double disc_y = 0;
    ost_discrepancy << sub_x << "  " << sub_y << "  "
                    << disc_x << "  " << disc_y << "\n";

    double init_x = aux_ptr->InitLoc().x;
    double init_y = aux_ptr->InitLoc().y;
    if (blk_rgn.region_id == 0) {
      double disp_x = blk_ptr->LLX() - init_x;
      double disp_y = blk_ptr->LLY() - init_y;
      ost_displacement << init_x << "  " << init_y << "  "
                       << disp_x << "  " << disp_y << "\n";
    }
  }
}

}
