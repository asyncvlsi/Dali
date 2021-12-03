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
#include "spacepartitioner.h"

namespace dali {

void SpacePartitioner::SetCircuit(Circuit *p_ckt) {
  DaliExpects(p_ckt != nullptr, "Partition space for a null Circuit?");
  p_ckt_ = p_ckt;
}

void SpacePartitioner::SetOutput(std::vector<ClusterStripe> *p_col_list) {
  DaliExpects(p_col_list != nullptr, "Save partitioning result to a nullptr?");
  p_col_list_ = p_col_list;
}

void SpacePartitioner::SetPartitionMode(StripePartitionMode stripe_mode) {
  stripe_mode_ = stripe_mode;
}

void SpacePartitioner::SetReservedSpaceToBoundaries(
    int l_space, int r_space, int b_space, int t_space
) {
  l_space_ = l_space;
  r_space_ = r_space;
  b_space_ = b_space;
  t_space_ = t_space;
}

void SpacePartitioner::FetchWellParameters() {
  Tech &tech = p_ckt_->tech();
  WellLayer &n_well_layer = tech.NwellLayer();
  double grid_value_x = p_ckt_->GridValueX();
  int same_well_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_well_spacing =
      std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ =
      (int) std::floor(n_well_layer.MaxPlugDist() / grid_value_x);
}

void SpacePartitioner::DetectAvailSpace() {
  if (!row_height_set_) {
    row_height_ = p_ckt_->RowHeightGridUnit();
  }
  tot_num_rows_ = (Top() - Bottom()) / row_height_;

  std::vector<std::vector<std::vector<int>>> macro_segments;
  macro_segments.resize(tot_num_rows_);
  std::vector<int> tmp(2, 0);
  bool out_of_range;
  for (auto &block: p_ckt_->Blocks()) {
    if (block.IsMovable()) continue;
    int ly = int(std::floor(block.LLY()));
    int uy = int(std::ceil(block.URY()));
    int lx = int(std::floor(block.LLX()));
    int ux = int(std::ceil(block.URX()));

    out_of_range = (ly >= Top()) || (uy <= Bottom())
        || (lx >= Right()) || (ux <= Left());

    if (out_of_range) continue;

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    tmp[0] = std::max(Left(), lx);
    tmp[1] = std::min(Right(), ux);
    if (tmp[1] > tmp[0]) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto &intervals: macro_segments) {
    MergeIntervals(intervals);
  }

  std::vector<std::vector<int>> intermediate_seg_rows;
  intermediate_seg_rows.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    if (macro_segments[i].empty()) {
      intermediate_seg_rows[i].push_back(Left());
      intermediate_seg_rows[i].push_back(Right());
      continue;
    }
    int segments_size = int(macro_segments[i].size());
    for (int j = 0; j < segments_size; ++j) {
      auto &interval = macro_segments[i][j];
      if (interval[0] == Left() && interval[1] < Right()) {
        intermediate_seg_rows[i].push_back(interval[1]);
      }

      if (interval[0] > Left()) {
        if (intermediate_seg_rows[i].empty()) {
          intermediate_seg_rows[i].push_back(Left());
        }
        intermediate_seg_rows[i].push_back(interval[0]);
        if (interval[1] < Right()) {
          intermediate_seg_rows[i].push_back(interval[1]);
        }
      }
    }
    if (intermediate_seg_rows[i].size() % 2 == 1) {
      intermediate_seg_rows[i].push_back(Right());
    }
  }

  white_space_in_rows_.resize(tot_num_rows_);
  int min_blk_width = int(p_ckt_->MinBlkWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    white_space_in_rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j]
          >= min_blk_width) {
        white_space_in_rows_[i].emplace_back(
            intermediate_seg_rows[i][j],
            intermediate_seg_rows[i][j + 1]
        );
      }
    }
  }
}

void SpacePartitioner::UpdateWhiteSpaceInCol(ClusterStripe &col) {
  SegI stripe_seg(col.LLX(), col.URX());
  col.white_space_.clear();
  col.white_space_.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    for (auto &seg: white_space_in_rows_[i]) {
      SegI *tmp_seg = stripe_seg.Joint(seg);
      if (tmp_seg != nullptr) {
        /*
        if (tmp_seg->lo - seg.lo < max_cell_width_ * 2 + well_spacing_) {
            if (tmp_seg->hi - seg.lo
                < stripe_width_factor_ * max_unplug_length_) {
                tmp_seg->lo = seg.lo;
            }
        }
        if (seg.hi - tmp_seg->hi
            < max_cell_width_ * 2 + well_spacing_) {
            if (seg.hi - tmp_seg->lo
                < stripe_width_factor_ * max_unplug_length_) {
                tmp_seg->hi = seg.hi;
            }
        }
        if (tmp_seg->Span() < max_cell_width_ * 2
            && tmp_seg->Span() < seg.Span()) {
            continue;
        }
        */
        col.white_space_[i].push_back(*tmp_seg);
      }
      delete tmp_seg;
    }
  }
}

void SpacePartitioner::DecomposeSpaceToSimpleStripes() {
  for (auto &col: *p_col_list_) {
    for (int i = 0; i < tot_num_rows_; ++i) {
      for (auto &seg: col.white_space_[i]) {
        int y_loc = RowToLoc(i);
        Stripe *stripe = col.GetStripeMatchSeg(seg, y_loc);
        if (stripe == nullptr) {
          col.stripe_list_.emplace_back();
          stripe = &(col.stripe_list_.back());
          stripe->lx_ = seg.lo;
          stripe->width_ = seg.Span();
          stripe->ly_ = y_loc;
          stripe->height_ = row_height_;
          stripe->contour_ = y_loc;
          stripe->front_cluster_ = nullptr;
          stripe->used_height_ = 0;
          stripe->max_blk_capacity_per_cluster_ =
              stripe->width_ / p_ckt_->MinBlkWidth();
        } else {
          stripe->height_ += row_height_;
        }
      }
    }
  }

  //col_list_[tot_col_num_ - 1].stripe_list_[0].width_ =
  //    RegionRight() - col_list_[tot_col_num_ - 1].stripe_list_[0].LLX() - well_spacing_;
  //col_list_[tot_col_num_ - 1].stripe_list_[0].max_blk_capacity_per_cluster_ =
  //    col_list_[tot_col_num_ - 1].stripe_list_[0].width_ / circuit_ptr_->MinBlkWidth();
  /*for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      stripe.height_ -= row_height_;
    }
  }*/

  //PlotSimpleStripes();
  //PlotAvailSpaceInCols();
}

void SpacePartitioner::AssignBlockToColBasedOnWhiteSpace() {
  // assign blocks to columns
  std::vector<Block> &block_list = p_ckt_->Blocks();
  std::vector<ClusterStripe> &col_list = *p_col_list_;
  int sz = (int) block_list.size();
  std::vector<int> block_column_assign(sz, -1);
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list[i].block_count_ = 0;
    col_list[i].block_list_.clear();
  }

  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    int col_num = LocToCol((int) std::round(block_list[i].X()));

    std::vector<int> pos_col;
    std::vector<double> distance;
    if (col_num > 0) {
      pos_col.push_back(col_num - 1);
      distance.push_back(0);
    }
    pos_col.push_back(col_num);
    distance.push_back(0);
    if (col_num < tot_col_num_ - 1) {
      pos_col.push_back(col_num + 1);
      distance.push_back(0);
    }

    Stripe *stripe = nullptr;
    double min_dist = DBL_MAX;
    for (auto &num: pos_col) {
      double tmp_dist;
      Stripe *res = col_list[num].GetStripeClosestToBlk(
          &block_list[i], tmp_dist
      );
      if (tmp_dist < min_dist) {
        stripe = res;
        col_num = num;
        min_dist = tmp_dist;
      }
    }
    if (stripe != nullptr) {
      col_list[col_num].block_count_++;
      block_column_assign[i] = col_num;
    } else {
      DaliExpects(false, "Cannot find a column to place cell: "
          + block_list[i].Name());
    }
  }
  for (int i = 0; i < tot_col_num_; ++i) {
    int capacity = col_list[i].block_count_;
    col_list[i].block_list_.reserve(capacity);
  }

  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    int col_num = block_column_assign[i];
    if (col_num >= 0) {
      col_list[col_num].block_list_.push_back(&block_list[i]);
    }
  }

  for (auto &col: col_list) {
    col.AssignBlockToSimpleStripe();
  }
}

bool SpacePartitioner::StartPartitioning() {
  DaliExpects(p_ckt_ != nullptr, "Circuit is not set");
  DaliExpects(p_col_list_ != nullptr, "Output location is not set");

  // initialize row height and white space segments
  DetectAvailSpace();

  FetchWellParameters();

  std::vector<ClusterStripe> &col_list = *p_col_list_;
  // find the maximum width among movable cells
  max_cell_width_ = 0;
  for (auto &blk: p_ckt_->Blocks()) {
    if (blk.IsMovable()) {
      max_cell_width_ = std::max(max_cell_width_, blk.Width());
    }
  }
  BOOST_LOG_TRIVIAL(info) << "Max movable cell width: "
                          << max_cell_width_ << "\n";

  // determine the width of columns
  if (cluster_width_ <= 0) {
    BOOST_LOG_TRIVIAL(info)
      << "Using default cluster width: 2*max_unplug_length_\n";
    stripe_width_ =
        (int) std::round(max_unplug_length_ * stripe_width_factor_);
  } else {
    DaliWarns(cluster_width_ < max_unplug_length_,
              "Specified cluster width is smaller than max_unplug_length_, space is wasted, may not be able to successfully complete well legalization");
    stripe_width_ = cluster_width_;
  }
  stripe_width_ = stripe_width_ + well_spacing_;
  int region_width = Right() - Left();
  int region_height = Top() - Bottom();
  if (stripe_width_ > region_width) {
    stripe_width_ = region_width;
  }
  tot_col_num_ = std::ceil(region_width / (double) stripe_width_);
  BOOST_LOG_TRIVIAL(info) << "Total number of cluster columns: "
                          << tot_col_num_ << "\n";
  int max_clusters_per_col = region_height / p_ckt_->MinBlkHeight();
  col_list.resize(tot_col_num_);
  stripe_width_ = region_width / tot_col_num_;
  BOOST_LOG_TRIVIAL(info) << "Cluster width: "
                          << stripe_width_ * p_ckt_->GridValueX() << " um, "
                          << stripe_width_ << "\n";
  DaliWarns(stripe_width_ < max_cell_width_,
            "Maximum cell width is longer than cluster width?");
  for (int i = 0; i < tot_col_num_; ++i) {
    col_list[i].lx_ = Left() + i * stripe_width_;
    col_list[i].width_ = stripe_width_ - well_spacing_;
    DaliExpects(col_list[i].width_ > 0,
                "CELL configuration is problematic, leading to non-positive column width");
    UpdateWhiteSpaceInCol(col_list[i]);
  }
  if (stripe_mode_ == SCAVENGE) {
    col_list.back().width_ = Right() - col_list.back().lx_;
    UpdateWhiteSpaceInCol(col_list.back());
  }
  DecomposeSpaceToSimpleStripes();
  //cluster_list_.reserve(tot_col_num_ * max_clusters_per_col);

  //BOOST_LOG_TRIVIAL(info)  <<"left: %d, right: %d\n", left_, right_);
  BOOST_LOG_TRIVIAL(info)
    << "Maximum possible number of clusters in a column: "
    << max_clusters_per_col << "\n";

  AssignBlockToColBasedOnWhiteSpace();

  return true;
}

void SpacePartitioner::PlotAvailSpace(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << "\t"
      << Right() << "\t"
      << Right() << "\t"
      << Left() << "\t"
      << Bottom() << "\t"
      << Bottom() << "\t"
      << Top() << "\t"
      << Top() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto &row = white_space_in_rows_[i];
    for (auto &seg: row) {
      ost << seg.lo << "\t"
          << seg.hi << "\t"
          << seg.hi << "\t"
          << seg.lo << "\t"
          << i * row_height_ + Bottom() << "\t"
          << i * row_height_ + Bottom() << "\t"
          << (i + 1) * row_height_ + Bottom() << "\t"
          << (i + 1) * row_height_ + Bottom() << "\t"
          << 1 << "\t"
          << 1 << "\t"
          << 1 << "\n";
    }
  }

  for (auto &block: p_ckt_->Blocks()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 1 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}

void SpacePartitioner::PlotAvailSpaceInCols(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << "\t"
      << Right() << "\t"
      << Right() << "\t"
      << Left() << "\t"
      << Bottom() << "\t"
      << Bottom() << "\t"
      << Top() << "\t"
      << Top() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (auto &col: *p_col_list_) {
    for (int i = 0; i < tot_num_rows_; ++i) {
      auto &row = col.white_space_[i];
      for (auto &seg: row) {
        ost << seg.lo << "\t"
            << seg.hi << "\t"
            << seg.hi << "\t"
            << seg.lo << "\t"
            << i * row_height_ + Bottom() << "\t"
            << i * row_height_ + Bottom() << "\t"
            << (i + 1) * row_height_ + Bottom() << "\t"
            << (i + 1) * row_height_ + Bottom() << "\t"
            << 0 << "\t"
            << 1 << "\t"
            << 1 << "\n";
      }
    }
  }

  for (auto &block: p_ckt_->Blocks()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 0 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}

void SpacePartitioner::PlotSimpleStripes(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << "\t"
      << Right() << "\t"
      << Right() << "\t"
      << Left() << "\t"
      << Bottom() << "\t"
      << Bottom() << "\t"
      << Top() << "\t"
      << Top() << "\t"
      << 1 << "\t"
      << 1 << "\t"
      << 1 << "\n";
  for (auto &col: *p_col_list_) {
    for (auto &stripe: col.stripe_list_) {
      ost << stripe.LLX() << "\t"
          << stripe.URX() << "\t"
          << stripe.URX() << "\t"
          << stripe.LLX() << "\t"
          << stripe.LLY() << "\t"
          << stripe.LLY() << "\t"
          << stripe.URY() << "\t"
          << stripe.URY() << "\t"
          << 0.8 << "\t"
          << 0.8 << "\t"
          << 0.8 << "\n";
    }
  }

  for (auto &block: p_ckt_->Blocks()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\t"
        << 0 << "\t"
        << 1 << "\t"
        << 1 << "\n";
  }
}

int SpacePartitioner::Left() const {
  return p_ckt_->RegionLLX() + l_space_;
}

int SpacePartitioner::Right() const {
  return p_ckt_->RegionURX() - r_space_;
}

int SpacePartitioner::Bottom() const {
  return p_ckt_->RegionLLY() + b_space_;
}

int SpacePartitioner::Top() const {
  return p_ckt_->RegionURY() - t_space_;
}

int SpacePartitioner::StartRow(int y_loc) const {
  return (y_loc - Bottom()) / row_height_;
}

int SpacePartitioner::EndRow(int y_loc) const {
  int relative_y = y_loc - Bottom();
  int res = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --res;
  }
  return res;
}

int SpacePartitioner::RowToLoc(int row_num, int displacement) {
  return row_num * row_height_ + Bottom() + displacement;
}

int SpacePartitioner::LocToCol(int x) {
  int col_num = (x - Left()) / stripe_width_;
  if (col_num < 0) {
    col_num = 0;
  }
  if (col_num >= tot_col_num_) {
    col_num = tot_col_num_ - 1;
  }
  return col_num;
}

}
