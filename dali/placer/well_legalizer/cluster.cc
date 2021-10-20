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
        blk_ptr->SetLLY(ly_ + p_well_height_ - well->Pheight());
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
        blk->SetLLX(current_x);
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
        blk->SetLLX(current_x);
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
        blk->SetLLX(res_x);
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
        blk->SetURX(res_x);
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
        BlockOrient orient = is_orient_N_ ? N : FS;
        double y_flip_axis = ly_ + height_ / 2.0;
        for (auto &blk_ptr: blk_list_) {
            double ly_to_axis = y_flip_axis - blk_ptr->LLY();
            blk_ptr->SetOrient(orient);
            blk_ptr->SetURY(y_flip_axis + ly_to_axis);
        }
    }
}

void Cluster::InsertWellTapCell(Block &tap_cell, int loc) {
    tap_cell_ = &tap_cell;
    blk_list_.emplace_back(tap_cell_);
    tap_cell_->SetCenterX(loc);
    auto *well = tap_cell.TypePtr()->WellPtr();
    int p_well_height = well->Pheight();
    int n_well_height = well->Nheight();
    if (is_orient_N_) {
        tap_cell.SetOrient(N);
        tap_cell.SetLLY(ly_ + p_well_height_ - p_well_height);
    } else {
        tap_cell.SetOrient(FS);
        tap_cell.SetLLY(ly_ + n_well_height_ - n_well_height);
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
        blk->SetLLX(current_x);
        blk->SetCenterY(CenterY());
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

        BlockSegment *cur_seg = &(segments[seg_sz - 1]);
        BlockSegment *prev_seg = &(segments[seg_sz - 2]);
        //std::cout << prev_seg->IsNotOnLeft(*cur_seg) << " ";
        while (prev_seg->IsNotOnLeft(*cur_seg)) {
            prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
            segments.pop_back();

            seg_sz = segments.size();
            if (seg_sz == 1) break;
            cur_seg = &(segments[seg_sz - 1]);
            prev_seg = &(segments[seg_sz - 2]);
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

void Cluster::UpdateMinDisplacementLLY() {
    DaliExpects(blk_list_.size() == blk_initial_location_.size(),
                "Block count does not equal initial location count\n");
    double sum = 0;
    for (auto &init_loc : blk_initial_location_) {
        double init_np_boundary = init_loc.y;
        sum += init_np_boundary;
    }
    min_displacement_lly_ = sum / (int) (blk_initial_location_.size()) - PHeight();
}

double Cluster::MinDisplacementLLY() const{
    return min_displacement_lly_;
}

void ClusterSegment::Merge(ClusterSegment &sc, int lower_bound, int upper_bound) {
    int sz = (int) sc.cluster_list.size();
    for (int i = 0; i < sz; ++i) {
        cluster_list.push_back(sc.cluster_list[i]);
    }
    height_ += sc.Height();

    sz = (int) cluster_list.size();
    int anchor_size = 0;
    for (auto &cluster_ptr: cluster_list) {
        anchor_size += (int) cluster_ptr->blk_list_.size();
    }
    std::vector<double> anchor;
    anchor.reserve(anchor_size);
    int accumulative_d = 0;
    for (int i = 0; i < sz; ++i) {
        for (auto &init_loc: cluster_list[i]->blk_initial_location_) {
            double init_np_boundary = init_loc.y;
            anchor.push_back(init_np_boundary - accumulative_d);
        }
        accumulative_d += cluster_list[i]->NHeight();
        if (i + 1 < sz) {
            accumulative_d += cluster_list[i + 1]->PHeight();
        } else {
            accumulative_d += cluster_list[0]->PHeight();
        }
    }
    DaliExpects(height_ == accumulative_d,
                "Something is wrong, height does not match, ClusterSegment::Merge()");

    long double sum = 0;
    for (auto &num: anchor) {
        sum += num;
    }
    int first_np_boundary = (int) std::round(sum / anchor_size);

    ly_ = first_np_boundary - cluster_list[0]->PHeight();
    if (ly_ < lower_bound) {
        ly_ = lower_bound;
    }
    if (ly_ + height_ > upper_bound) {
        ly_ = upper_bound - height_;
    }
}

void ClusterSegment::UpdateClusterLocation() {
    int cur_y = ly_;
    int sz = (int) cluster_list.size();
    for (int i = 0; i < sz; ++i) {
        cluster_list[i]->SetLLY(cur_y);
        cluster_list[i]->UpdateBlockLocY();
        cur_y += cluster_list[i]->Height();
    }
}

}