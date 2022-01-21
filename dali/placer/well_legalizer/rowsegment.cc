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

void RowSegment::MinDisplacementLegalization() {
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
  for (auto &blk_rgn: blk_regions_) {
    auto aux_ptr = static_cast<LgBlkAux *>(blk_rgn.p_blk->AuxPtr());
    vars.emplace_back(blk_rgn.p_blk->Width(), aux_ptr->InitLoc().x, 1.0);
    vars.back().blk_rgn = blk_rgn;
  }

  // get optimized locations and store them in vars
  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  // set block locations from vars
  for (auto &var: vars) {
    var.UpdateBlkLocation();
  }
}

void RowSegment::SetOptimalAnchorWeight(double weight) {
  opt_anchor_weight_ = weight;
}

std::vector<BlkDispVar> RowSegment::OptimizeQuadraticDisplacement(double lambda) {
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
    if (region_cnt > 1) {
      vars.back().SetAnchor(
          aux_ptr->AverageLoc(),
          (1 - lambda) * (1 + 1000*std::fabs(aux_ptr->AverageLoc() - aux_ptr->SubLocs()[blk_rgn.region_id])) // / region_cnt
      );
    }
  }

  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  return vars;
}

std::vector<BlkDispVar> RowSegment::OptimizeLinearDisplacement(double lambda) {
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
  int sub_cell_cnt = 0;
  double sum_discrepancy = 0;
  for (auto &blk_rgn: blk_regions_) {
    Block *blk_ptr = blk_rgn.p_blk;
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    int region_cnt = blk_ptr->TypePtr()->WellPtr()->RegionCount();
    double average_loc = aux_ptr->AverageLoc();
    double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
    double tmp_discrepancy = std::fabs(average_loc - sub_loc);
    sum_discrepancy += tmp_discrepancy;
    ++sub_cell_cnt;
  }
  double ave_discrepancy = sum_discrepancy / sub_cell_cnt;
  if (ave_discrepancy < 1e-5) {
    ave_discrepancy = 1;
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
    double sub_loc = aux_ptr->SubLocs()[blk_rgn.region_id];
    double average_loc = aux_ptr->AverageLoc();
    double tmp_discrepancy = std::fabs(average_loc - sub_loc);
    double weight_discrepancy = exp(tmp_discrepancy/ave_discrepancy);
    vars.back().SetAnchor(
        aux_ptr->AverageLoc(),
        (1 - lambda) * weight_discrepancy // / region_cnt
    );
  }

  MinimizeLinearDisplacement(vars, LLX(), URX());

  return vars;
}

void RowSegment::GenSubCellTable(
    std::ofstream &ost_sub_cell,
    std::ofstream &ost_discrepancy,
    std::ofstream &ost_displacement,
    double row_ly,
    double row_uy
) {
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
      double disp_x = blk_ptr->LLX() - aux_ptr->AverageLoc();
      double disp_y = blk_ptr->LLY() - init_y;
      ost_displacement << init_x << "  " << init_y << "  "
                       << disp_x << "  " << disp_y << "\n";
    }
  }
}

}
