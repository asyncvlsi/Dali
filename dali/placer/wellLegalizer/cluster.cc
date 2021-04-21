//
// Created by yihang on 4/12/21.
//

#include "cluster.h"

#include <algorithm>

namespace dali {

void Cluster::ShiftBlockX(int x_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
  }
}

void Cluster::ShiftBlockY(int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseY(y_disp);
  }
}

void Cluster::ShiftBlock(int x_disp, int y_disp) {
  for (auto &blk_ptr: blk_list_) {
    blk_ptr->IncreaseX(x_disp);
    blk_ptr->IncreaseY(y_disp);
  }
}

void Cluster::UpdateBlockLocY() {
  //Assert(p_well_height_ + n_well_height_ == height_, "Inconsistency occurs: p_well_height + n_Well_height != height\n");
  for (auto &blk_ptr: blk_list_) {
    auto *well = blk_ptr->TypePtr()->WellPtr();
    blk_ptr->setLLY(ly_ + p_well_height_ - well->PHeight());
  }
}

void Cluster::LegalizeCompactX(int left) {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = left;
  for (auto &blk: blk_list_) {
    blk->setLLX(current_x);
    current_x += blk->Width();
  }
}

void Cluster::LegalizeCompactX() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->setLLX(current_x);
    current_x += blk->Width();
  }
}

void Cluster::LegalizeLooseX(int space_to_well_tap) {
  /****
   * Legalize this cluster using the extended Tetris legalization algorithm
   *
   * 1. legalize blocks from left
   * 2. if block contour goes out of the right boundary, legalize blocks from right
   *
   * if the total width of blocks in this cluster is smaller than the width of this cluster,
   * two-rounds legalization is enough to make the final result legal.
   * ****/

  if (blk_list_.empty()) {
    return;
  }
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int block_contour = lx_;
  int res_x;
  for (auto &blk: blk_list_) {
    res_x = std::max(block_contour, int(blk->LLX()));
    blk->setLLX(res_x);
    block_contour = int(blk->URX());
    if ((tap_cell_ != nullptr) && (blk->TypePtr() == tap_cell_->TypePtr())) {
      block_contour += space_to_well_tap;
    }
  }

  int ux = lx_ + width_;
  //if (block_contour > ux) {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->URX() > blk_ptr1->URX();
            });
  block_contour = ux;
  for (auto &blk: blk_list_) {
    res_x = std::min(block_contour, int(blk->URX()));
    blk->setURX(res_x);
    block_contour = int(blk->LLX());
    if ((tap_cell_ != nullptr) && (blk->TypePtr() == tap_cell_->TypePtr())) {
      block_contour -= space_to_well_tap;
    }
  }
  //}
}

void Cluster::SetOrient(bool is_orient_N) {
  if (is_orient_N_ != is_orient_N) {
    is_orient_N_ = is_orient_N;
    BlockOrient orient = is_orient_N_ ? N_ : FS_;
    double y_flip_axis = ly_ + height_ / 2.0;
    for (auto &blk_ptr: blk_list_) {
      double ly_to_axis = y_flip_axis - blk_ptr->LLY();
      blk_ptr->setOrient(orient);
      blk_ptr->setURY(y_flip_axis + ly_to_axis);
    }
  }
}

void Cluster::InsertWellTapCell(Block &tap_cell, int loc) {
  tap_cell_ = &tap_cell;
  blk_list_.emplace_back(tap_cell_);
  tap_cell_->setCenterX(loc);
  auto *well = tap_cell.TypePtr()->WellPtr();
  int p_well_height = well->PHeight();
  int n_well_height = well->NHeight();
  if (is_orient_N_) {
    tap_cell.setOrient(N_);
    tap_cell.setLLY(ly_ + p_well_height_ - p_well_height);
  } else {
    tap_cell.setOrient(FS_);
    tap_cell.setLLY(ly_ + n_well_height_ - n_well_height);
  }
}

void Cluster::UpdateBlockLocationCompact() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
  int current_x = lx_;
  for (auto &blk: blk_list_) {
    blk->setLLX(current_x);
    blk->setCenterY(CenterY());
    current_x += blk->Width();
  }
}

void Cluster::MinDisplacementLegalization() {
  std::sort(blk_list_.begin(),
            blk_list_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
                return blk_ptr0->X() < blk_ptr1->X();
            });

  std::vector<BlockSegment> segments;

  DaliExpects(blk_list_.size() == blk_initial_location_.size(),
              "Block number does not equal initial location number\n");

  size_t sz = blk_list_.size();
  int lower_bound = lx_;
  int upper_bound = lx_ + width_;
  //std::cout << sz << "--";
  for (size_t i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    Block *blk_ptr = blk_list_[i];
    double init_x = blk_initial_location_[i].x;
    if (init_x < lower_bound) {
      init_x = lower_bound;
    }
    if (init_x + blk_ptr->Width() > upper_bound) {
      init_x = upper_bound - blk_ptr->Width();
    }
    segments.emplace_back(blk_ptr, init_x);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    BlockSegment *cur_seg = &(segments[seg_sz-1]);
    BlockSegment *prev_seg = &(segments[seg_sz-2]);
    //std::cout << prev_seg->IsOnLeft(*cur_seg) << " ";
    while (prev_seg->IsOnLeft(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz-1]);
      prev_seg = &(segments[seg_sz-2]);
    }
  }

  //int count = 0;
  //std::cout << "...";
  for (auto &seg: segments) {
    seg.UpdateBlockLocation();
    //count += seg.blk_list.size();
    //std::cout << seg.blk_list.size() << " ";
    //seg.Report();
  }
  //std::cout << "--" << count << "\n";
}

}