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

#include "dali/placer/well_legalizer/blocksegment.h"
#include "dali/placer/well_legalizer/helper.h"
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
  for (auto &[blk_ptr, region_id]: blk_regions_) {
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    vars.emplace_back(blk_ptr->Width(), aux_ptr->InitLoc().x, 1.0);
    vars.back().blk_rgn.p_blk = blk_ptr;
    vars.back().blk_rgn.region_id = region_id;
  }

  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  for (auto &var: vars) {
    var.UpdateBlkLocation();
  }
}

void RowSegment::SetOptimalAnchorWeight(double weight) {
  opt_anchor_weight_ = weight;
}

void RowSegment::BuildQuadraticOptimizationProblem() {

}

std::vector<BlkDispVar> RowSegment::OptimizeQuadraticDisplacement() {
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
  for (auto &[blk_ptr, region_id]: blk_regions_) {
    auto aux_ptr = static_cast<LgBlkAux *>(blk_ptr->AuxPtr());
    vars.emplace_back(blk_ptr->Width(), aux_ptr->InitLoc().x, 1.0);
    vars.back().blk_rgn.p_blk = blk_ptr;
    vars.back().blk_rgn.region_id = region_id;
  }

  MinimizeQuadraticDisplacement(vars, LLX(), URX());

  return vars;
}

}
