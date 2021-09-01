//
// Created by Yihang Yang on 3/14/20.
//

#include "stdclusterwelllegalizer.h"

#include <algorithm>

namespace dali {

StdClusterWellLegalizer::StdClusterWellLegalizer() {
    max_unplug_length_ = 0;
    well_tap_cell_width_ = 0;
    stripe_width_ = 0;
    tot_col_num_ = 0;
    row_height_set_ = false;
}

StdClusterWellLegalizer::~StdClusterWellLegalizer() {
    well_tap_cell_ = nullptr;
}

void StdClusterWellLegalizer::LoadConf(std::string const &config_file) {
    config_read(config_file.c_str());
    std::string variable;

    variable = "dali.StdClusterWellLegalizer.stripe_width_factor_";
    if (config_exists(variable.c_str()) == 1) {
        stripe_width_factor_ = config_get_real(variable.c_str());
    }
    variable = "dali.StdClusterWellLegalizer.space_to_well_tap_";
    if (config_exists(variable.c_str()) == 1) {
        space_to_well_tap_ = config_get_int(variable.c_str());
    }
    variable = "dali.StdClusterWellLegalizer.max_iter_";
    if (config_exists(variable.c_str()) == 1) {
        max_iter_ = config_get_int(variable.c_str());
    }
}

void StdClusterWellLegalizer::CheckWellExistence() {
    for (auto &blk: *(circuit_->getBlockList())) {
        if (blk.IsMovable()) {
            DaliExpects(blk.TypePtr()->WellPtr() != nullptr, "Cannot find well info for cell: " + blk.Name());
        }
    }
}

void StdClusterWellLegalizer::DetectAvailSpace() {
    if (!row_height_set_) {
        row_height_ = circuit_->getINTRowHeight();
    }
    tot_num_rows_ = (top_ - bottom_) / row_height_;

    std::vector<std::vector<std::vector<int>>> macro_segments;
    macro_segments.resize(tot_num_rows_);
    std::vector<int> tmp(2, 0);
    bool out_of_range;
    for (auto &block: *BlockList()) {
        if (block.IsMovable()) continue;
        int ly = int(std::floor(block.LLY()));
        int uy = int(std::ceil(block.URY()));
        int lx = int(std::floor(block.LLX()));
        int ux = int(std::ceil(block.URX()));

        out_of_range = (ly >= RegionTop()) || (uy <= RegionBottom()) || (lx >= RegionRight()) || (ux <= RegionLeft());

        if (out_of_range) continue;

        int start_row = StartRow(ly);
        int end_row = EndRow(uy);

        start_row = std::max(0, start_row);
        end_row = std::min(tot_num_rows_ - 1, end_row);

        tmp[0] = std::max(RegionLeft(), lx);
        tmp[1] = std::min(RegionRight(), ux);
        if (tmp[1] > tmp[0]) {
            for (int i = start_row; i <= end_row; ++i) {
                macro_segments[i].push_back(tmp);
            }
        }
    }
    for (auto &intervals : macro_segments) {
        LGTetrisEx::MergeIntervals(intervals);
    }

    std::vector<std::vector<int>> intermediate_seg_rows;
    intermediate_seg_rows.resize(tot_num_rows_);
    for (int i = 0; i < tot_num_rows_; ++i) {
        if (macro_segments[i].empty()) {
            intermediate_seg_rows[i].push_back(left_);
            intermediate_seg_rows[i].push_back(right_);
            continue;
        }
        int segments_size = int(macro_segments[i].size());
        for (int j = 0; j < segments_size; ++j) {
            auto &interval = macro_segments[i][j];
            if (interval[0] == left_ && interval[1] < RegionRight()) {
                intermediate_seg_rows[i].push_back(interval[1]);
            }

            if (interval[0] > left_) {
                if (intermediate_seg_rows[i].empty()) {
                    intermediate_seg_rows[i].push_back(left_);
                }
                intermediate_seg_rows[i].push_back(interval[0]);
                if (interval[1] < RegionRight()) {
                    intermediate_seg_rows[i].push_back(interval[1]);
                }
            }
        }
        if (intermediate_seg_rows[i].size() % 2 == 1) {
            intermediate_seg_rows[i].push_back(right_);
        }
    }

    white_space_in_rows_.resize(tot_num_rows_);
    int min_blk_width = int(circuit_->MinBlkWidth());
    for (int i = 0; i < tot_num_rows_; ++i) {
        int len = int(intermediate_seg_rows[i].size());
        white_space_in_rows_[i].reserve(len / 2);
        for (int j = 0; j < len; j += 2) {
            if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j] >= min_blk_width) {
                white_space_in_rows_[i].emplace_back(intermediate_seg_rows[i][j], intermediate_seg_rows[i][j + 1]);
            }
        }
    }

    //PlotAvailSpace();
}

void StdClusterWellLegalizer::FetchNPWellParams() {
    auto tech = circuit_->getTech();
    DaliExpects(tech != nullptr, "No tech info found, well legalization cannot proceed!\n");

    auto n_well_layer = tech->GetNLayer();
    int same_well_spacing = std::ceil(n_well_layer->Spacing() / circuit_->GridValueX());
    int op_well_spacing = std::ceil(n_well_layer->OpSpacing() / circuit_->GridValueX());
    well_spacing_ = std::max(same_well_spacing, op_well_spacing);
    max_unplug_length_ = (int) std::floor(n_well_layer->MaxPlugDist() / circuit_->GridValueX());
    DaliExpects(!tech->WellTapCellRef().empty(),
                "Cannot find the definition of well tap cell, well legalization cannot proceed\n");
    well_tap_cell_ = tech->WellTapCellRef()[0];
    well_tap_cell_width_ = well_tap_cell_->Width();

    BOOST_LOG_TRIVIAL(info) << "Well max plug distance: "
                            << n_well_layer->MaxPlugDist() << " um, "
                            << max_unplug_length_ << " \n";
    BOOST_LOG_TRIVIAL(info) << "GridValueX: " << circuit_->GridValueX() << " um\n";
    BOOST_LOG_TRIVIAL(info) << "Well spacing: "
                            << n_well_layer->Spacing() << " um, "
                            << well_spacing_ << "\n";
    BOOST_LOG_TRIVIAL(info) << "Well tap cell width: " << well_tap_cell_width_ << "\n";

    well_tap_cell_ = (circuit_->getTech()->WellTapCellRef()[0]);
    auto *tap_cell_well = well_tap_cell_->WellPtr();
    tap_cell_p_height_ = tap_cell_well->PHeight();
    tap_cell_n_height_ = tap_cell_well->NHeight();
}

void StdClusterWellLegalizer::UpdateWhiteSpaceInCol(ClusterStripe &col) {
    SegI stripe_seg(col.LLX(), col.URX());
    col.white_space_.clear();

    col.white_space_.resize(tot_num_rows_);
    for (int i = 0; i < tot_num_rows_; ++i) {
        for (auto &seg: white_space_in_rows_[i]) {
            auto tmp_seg = stripe_seg.Joint(seg);
            if (tmp_seg != nullptr) {
                if (tmp_seg->lo - seg.lo < max_cell_width_ * 2 + well_spacing_) {
                    if (tmp_seg->hi - seg.lo < stripe_width_factor_ * max_unplug_length_) {
                        tmp_seg->lo = seg.lo;
                    }
                }
                if (seg.hi - tmp_seg->hi < max_cell_width_ * 2 + well_spacing_) {
                    if (seg.hi - tmp_seg->lo < stripe_width_factor_ * max_unplug_length_) {
                        tmp_seg->hi = seg.hi;
                    }
                }
                if (tmp_seg->Span() < max_cell_width_ * 2 && tmp_seg->Span() < seg.Span()) continue;;
                col.white_space_[i].push_back(*tmp_seg);
            }
            delete tmp_seg;
        }
    }
}

void StdClusterWellLegalizer::DecomposeToSimpleStripe() {
    for (auto &col: col_list_) {
        for (int i = 0; i < tot_num_rows_; ++i) {
            for (auto &seg: col.white_space_[i]) {
                int y_loc = RowToLoc(i);
                auto stripe = col.GetStripeMatchSeg(seg, y_loc);
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
                    stripe->max_blk_capacity_per_cluster_ = stripe->width_ / circuit_->MinBlkWidth();
                } else {
                    stripe->height_ += row_height_;
                }
            }
        }
    }

    //col_list_[tot_col_num_ - 1].stripe_list_[0].width_ =
    //    RegionRight() - col_list_[tot_col_num_ - 1].stripe_list_[0].LLX() - well_spacing_;
    //col_list_[tot_col_num_ - 1].stripe_list_[0].max_blk_capacity_per_cluster_ =
    //    col_list_[tot_col_num_ - 1].stripe_list_[0].width_ / circuit_->MinBlkWidth();
    /*for (auto &col: col_list_) {
      for (auto &stripe: col.stripe_list_) {
        stripe.height_ -= row_height_;
      }
    }*/

    //PlotSimpleStripes();
}

void StdClusterWellLegalizer::SaveInitialBlockLocation() {
    block_init_locations_.clear();

    std::vector<Block> &block_list = circuit_->BlockListRef();
    block_init_locations_.reserve(block_list.size());

    for (auto &block: block_list) {
        block_init_locations_.emplace_back(block.LLX(), block.LLY());
    }
}

void StdClusterWellLegalizer::Initialize(int cluster_width) {
    CheckWellExistence();

    // fetch parameters about N/P-well
    FetchNPWellParams();

    // temporarily change left and right boundary to reserve space
    //BOOST_LOG_TRIVIAL(trace) << "left: %d, right: %d, width: %d\n", left_, right_, RegionWidth());
    left_ += well_spacing_;
    right_ -= well_spacing_;
    //BOOST_LOG_TRIVIAL(trace) << "left: %d, right: %d, width: %d\n", left_, right_, RegionWidth());

    // initialize row height and white space segments
    DetectAvailSpace();

    max_cell_width_ = 0;
    for (auto &blk: *BlockList()) {
        if (blk.IsMovable()) {
            max_cell_width_ = std::max(max_cell_width_, blk.Width());
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Max cell width: " << max_cell_width_ << "\n";

    if (cluster_width <= 0) {
        BOOST_LOG_TRIVIAL(info) << "Using default cluster width: 2*max_unplug_length_\n";
        stripe_width_ = (int) std::round(max_unplug_length_ * stripe_width_factor_);
    } else {
        if (cluster_width < max_unplug_length_) {
            BOOST_LOG_TRIVIAL(warning) << "WARNING:\n"
                                       << "  Specified cluster width is smaller than max_unplug_length_, "
                                       << "  space is wasted, may not be able to successfully complete well legalization\n";
        }
        stripe_width_ = cluster_width;
    }

    stripe_width_ = stripe_width_ + well_spacing_;
    if (stripe_width_ > RegionWidth()) {
        stripe_width_ = RegionWidth();
    }
    tot_col_num_ = std::ceil(RegionWidth() / (double) stripe_width_);
    BOOST_LOG_TRIVIAL(info) << "Total number of cluster columns: " << tot_col_num_ << "\n";

    //BOOST_LOG_TRIVIAL(info)   << RegionHeight() << "  " << circuit_->MinBlkHeight() << "\n";
    int max_clusters_per_col = RegionHeight() / circuit_->MinBlkHeight();
    col_list_.resize(tot_col_num_);
    stripe_width_ = RegionWidth() / tot_col_num_;
    BOOST_LOG_TRIVIAL(info) << "Cluster width: " << stripe_width_ * circuit_->GridValueX() << " um\n";
    for (int i = 0; i < tot_col_num_; ++i) {
        col_list_[i].lx_ = RegionLeft() + i * stripe_width_;
        col_list_[i].width_ = stripe_width_ - well_spacing_;
        UpdateWhiteSpaceInCol(col_list_[i]);
    }
    if (stripe_mode_ == SCAVENGE) {
        col_list_.back().width_ = RegionRight() - col_list_.back().lx_;
        UpdateWhiteSpaceInCol(col_list_.back());
    }
    DecomposeToSimpleStripe();
    //PlotAvailSpaceInCols();
    //cluster_list_.reserve(tot_col_num_ * max_clusters_per_col);

    // restore left and right boundaries back
    left_ -= well_spacing_;
    right_ += well_spacing_;
    //BOOST_LOG_TRIVIAL(info)  <<"left: %d, right: %d\n", left_, right_);
    BOOST_LOG_TRIVIAL(info) << "Maximum possible number of clusters in a column: " << max_clusters_per_col << "\n";

    index_loc_list_.resize(BlockList()->size());
    // temporarily change left and right boundary to reserve space

}

void StdClusterWellLegalizer::AssignBlockToColBasedOnWhiteSpace() {
    // assign blocks to columns
    int sz = (int) BlockList()->size();
    std::vector<int> block_column_assign(sz, -1);
    for (int i = 0; i < tot_col_num_; ++i) {
        col_list_[i].block_count_ = 0;
        col_list_[i].block_list_.clear();
    }

    std::vector<Block> &block_list = *BlockList();
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
            auto res = col_list_[num].GetStripeClosestToBlk(&block_list[i], tmp_dist);
            if (tmp_dist < min_dist) {
                stripe = res;
                col_num = num;
                min_dist = tmp_dist;
            }
        }
        if (stripe != nullptr) {
            col_list_[col_num].block_count_++;
            block_column_assign[i] = col_num;
        }
    }
    for (int i = 0; i < tot_col_num_; ++i) {
        int capacity = col_list_[i].block_count_;
        col_list_[i].block_list_.reserve(capacity);
    }

    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) continue;
        int col_num = block_column_assign[i];
        if (col_num >= 0) {
            col_list_[col_num].block_list_.push_back(&block_list[i]);
        }
    }

    for (auto &col:col_list_) {
        col.AssignBlockToSimpleStripe();
    }
}

void StdClusterWellLegalizer::AppendBlockToColBottomUp(Stripe &stripe, Block &blk) {
    bool is_no_cluster_in_col = (stripe.contour_ == stripe.LLY());
    bool is_new_cluster_needed = is_no_cluster_in_col;
    if (!is_new_cluster_needed) {
        bool is_not_in_top_cluster = stripe.contour_ <= blk.LLY();
        bool is_top_cluster_full = stripe.front_cluster_->UsedSize() + blk.Width() > stripe.width_;
        is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
    }

    int width = blk.Width();
    int init_y = (int) std::round(blk.LLY());
    init_y = std::max(init_y, stripe.contour_);

    Cluster *front_cluster;
    auto *blk_well = blk.TypePtr()->WellPtr();
    int p_well_height = blk_well->PHeight();
    int n_well_height = blk_well->NHeight();
    if (is_new_cluster_needed) {
        stripe.cluster_list_.emplace_back();
        front_cluster = &(stripe.cluster_list_.back());
        front_cluster->blk_list_.reserve(stripe.max_blk_capacity_per_cluster_);
        front_cluster->AddBlock(&blk);
        //int num_of_tap_cell = (int) std::ceil(stripe.Width() / max_unplug_length_);
        int num_of_tap_cell = 2;
        front_cluster->SetUsedSize(
            width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
        front_cluster->UpdateWellHeightFromBottom(tap_cell_p_height_, tap_cell_n_height_);
        front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
        front_cluster->SetLLY(init_y);
        front_cluster->SetLLX(stripe.LLX());
        front_cluster->SetWidth(stripe.Width());

        stripe.front_cluster_ = front_cluster;
        stripe.cluster_count_ += 1;
        stripe.used_height_ += front_cluster->Height();
    } else {
        front_cluster = stripe.front_cluster_;
        front_cluster->AddBlock(&blk);
        front_cluster->UseSpace(width);
        if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
            int old_height = front_cluster->Height();
            front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
            stripe.used_height_ += front_cluster->Height() - old_height;
        }
    }
    stripe.contour_ = front_cluster->URY();
}

void StdClusterWellLegalizer::AppendBlockToColTopDown(Stripe &stripe, Block &blk) {
    bool is_no_cluster = stripe.cluster_list_.empty();
    bool is_new_cluster_needed = is_no_cluster;
    if (!is_new_cluster_needed) {
        bool is_not_in_top_cluster = stripe.contour_ >= blk.URY();
        bool is_top_cluster_full = stripe.front_cluster_->UsedSize() + blk.Width() > stripe.width_;
        is_new_cluster_needed = is_not_in_top_cluster || is_top_cluster_full;
    }

    int width = blk.Width();
    int init_y = (int) std::round(blk.URY());
    init_y = std::min(init_y, stripe.contour_);

    Cluster *front_cluster;
    auto *blk_well = blk.TypePtr()->WellPtr();
    int p_well_height = blk_well->PHeight();
    int n_well_height = blk_well->NHeight();
    if (is_new_cluster_needed) {
        stripe.cluster_list_.emplace_back();
        front_cluster = &(stripe.cluster_list_.back());
        front_cluster->blk_list_.reserve(stripe.max_blk_capacity_per_cluster_);
        front_cluster->AddBlock(&blk);
        //int num_of_tap_cell = (int) std::ceil(stripe.Width() / max_unplug_length_);
        int num_of_tap_cell = 2;
        front_cluster->SetUsedSize(
            width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
        front_cluster->UpdateWellHeightFromTop(tap_cell_p_height_, tap_cell_n_height_);
        front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
        front_cluster->SetURY(init_y);
        front_cluster->SetLLX(stripe.LLX());
        front_cluster->SetWidth(stripe.Width());

        stripe.front_cluster_ = front_cluster;
        stripe.cluster_count_ += 1;
        stripe.used_height_ += front_cluster->Height();
    } else {
        front_cluster = stripe.front_cluster_;
        front_cluster->AddBlock(&blk);
        front_cluster->UseSpace(width);
        if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
            int old_height = front_cluster->Height();
            front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
            stripe.used_height_ += front_cluster->Height() - old_height;
        }
    }
    stripe.contour_ = front_cluster->LLY();
}

void StdClusterWellLegalizer::AppendBlockToColBottomUpCompact(Stripe &stripe, Block &blk) {
    bool is_new_cluster_needed = (stripe.contour_ == stripe.LLY());
    if (!is_new_cluster_needed) {
        bool is_top_cluster_full = stripe.front_cluster_->UsedSize() + blk.Width() > stripe.width_;
        is_new_cluster_needed = is_top_cluster_full;
    }

    int width = blk.Width();
    int init_y = (int) std::round(blk.LLY());
    init_y = std::max(init_y, stripe.contour_);

    Cluster *front_cluster;
    auto *well = blk.TypePtr()->WellPtr();
    int p_well_height = well->PHeight();
    int n_well_height = well->NHeight();
    if (is_new_cluster_needed) {
        stripe.cluster_list_.emplace_back();
        front_cluster = &(stripe.cluster_list_.back());
        front_cluster->blk_list_.reserve(stripe.max_blk_capacity_per_cluster_);
        front_cluster->AddBlock(&blk);
        //int num_of_tap_cell = (int) std::ceil(stripe.Width() / max_unplug_length_);
        int num_of_tap_cell = 2;
        front_cluster->SetUsedSize(
            width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
        front_cluster->UpdateWellHeightFromBottom(tap_cell_p_height_, tap_cell_n_height_);
        front_cluster->SetLLY(init_y);
        front_cluster->SetLLX(stripe.LLX());
        front_cluster->SetWidth(stripe.Width());
        front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);

        stripe.front_cluster_ = front_cluster;
        stripe.cluster_count_ += 1;
        stripe.used_height_ += front_cluster->Height();
    } else {
        front_cluster = stripe.front_cluster_;
        front_cluster->AddBlock(&blk);
        front_cluster->UseSpace(width);
        if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
            int old_height = front_cluster->Height();
            front_cluster->UpdateWellHeightFromBottom(p_well_height, n_well_height);
            stripe.used_height_ += front_cluster->Height() - old_height;
        }
    }
    stripe.contour_ = front_cluster->URY();
}

void StdClusterWellLegalizer::AppendBlockToColTopDownCompact(Stripe &stripe, Block &blk) {
    bool is_new_cluster_needed = (stripe.contour_ == stripe.URY());
    if (!is_new_cluster_needed) {
        bool is_top_cluster_full = stripe.front_cluster_->UsedSize() + blk.Width() > stripe.width_;
        is_new_cluster_needed = is_top_cluster_full;
    }

    int width = blk.Width();
    int init_y = (int) std::round(blk.URY());
    init_y = std::min(init_y, stripe.contour_);

    Cluster *front_cluster;
    auto *well = blk.TypePtr()->WellPtr();
    int p_well_height = well->PHeight();
    int n_well_height = well->NHeight();
    if (is_new_cluster_needed) {
        stripe.cluster_list_.emplace_back();
        front_cluster = &(stripe.cluster_list_.back());
        front_cluster->blk_list_.reserve(stripe.max_blk_capacity_per_cluster_);
        front_cluster->AddBlock(&blk);
        //int num_of_tap_cell = (int) std::ceil(stripe.Width() / max_unplug_length_);
        int num_of_tap_cell = 2;
        front_cluster->SetUsedSize(
            width + num_of_tap_cell * well_tap_cell_width_ + num_of_tap_cell * space_to_well_tap_);
        front_cluster->UpdateWellHeightFromTop(tap_cell_p_height_, tap_cell_n_height_);
        front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
        front_cluster->SetURY(init_y);
        front_cluster->SetLLX(stripe.LLX());
        front_cluster->SetWidth(stripe.Width());

        stripe.front_cluster_ = front_cluster;
        stripe.cluster_count_ += 1;
        stripe.used_height_ += front_cluster->Height();
    } else {
        front_cluster = stripe.front_cluster_;
        front_cluster->AddBlock(&blk);
        front_cluster->UseSpace(width);
        if (p_well_height > front_cluster->PHeight() || n_well_height > front_cluster->NHeight()) {
            int old_height = front_cluster->Height();
            front_cluster->UpdateWellHeightFromTop(p_well_height, n_well_height);
            stripe.used_height_ += front_cluster->Height() - old_height;
        }
    }
    stripe.contour_ = front_cluster->LLY();
}

bool StdClusterWellLegalizer::StripeLegalizationBottomUp(Stripe &stripe) {
    stripe.cluster_list_.clear();
    stripe.contour_ = stripe.LLY();
    stripe.used_height_ = 0;
    stripe.cluster_count_ = 0;
    stripe.front_cluster_ = nullptr;
    stripe.is_bottom_up_ = true;

    std::sort(stripe.block_list_.begin(),
              stripe.block_list_.end(),
              [](const Block *lhs, const Block *rhs) {
                  return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
              });
    for (auto &blk_ptr: stripe.block_list_) {
        if (blk_ptr->IsFixed()) continue;
        AppendBlockToColBottomUp(stripe, *blk_ptr);
    }

    for (auto &cluster: stripe.cluster_list_) {
        cluster.UpdateBlockLocY();
    }

    return stripe.contour_ <= stripe.URY();
}

bool StdClusterWellLegalizer::StripeLegalizationTopDown(Stripe &stripe) {
    stripe.cluster_list_.clear();
    stripe.contour_ = stripe.URY();
    stripe.used_height_ = 0;
    stripe.cluster_count_ = 0;
    stripe.front_cluster_ = nullptr;
    stripe.is_bottom_up_ = false;

    std::sort(stripe.block_list_.begin(),
              stripe.block_list_.end(),
              [](const Block *lhs, const Block *rhs) {
                  return (lhs->URY() > rhs->URY()) || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
              });
    for (auto &blk_ptr: stripe.block_list_) {
        if (blk_ptr->IsFixed()) continue;
        AppendBlockToColTopDown(stripe, *blk_ptr);
    }

    for (auto &cluster: stripe.cluster_list_) {
        cluster.UpdateBlockLocY();
    }

    /*BOOST_LOG_TRIVIAL(info)   << "Reverse clustering: ";
    if (stripe.contour_ >= RegionLLY()) {
      BOOST_LOG_TRIVIAL(info)   << "success\n";
    } else {
      BOOST_LOG_TRIVIAL(info)   << "fail\n";
    }*/

    return stripe.contour_ >= stripe.LLY();
}

bool StdClusterWellLegalizer::StripeLegalizationBottomUpCompact(Stripe &stripe) {
    stripe.cluster_list_.clear();
    stripe.contour_ = RegionBottom();
    stripe.used_height_ = 0;
    stripe.cluster_count_ = 0;
    stripe.front_cluster_ = nullptr;
    stripe.is_bottom_up_ = true;

    std::sort(stripe.block_list_.begin(),
              stripe.block_list_.end(),
              [](const Block *lhs, const Block *rhs) {
                  return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
              });
    for (auto &blk_ptr: stripe.block_list_) {
        if (blk_ptr->IsFixed()) continue;
        AppendBlockToColBottomUpCompact(stripe, *blk_ptr);
    }

    for (auto &cluster: stripe.cluster_list_) {
        cluster.UpdateBlockLocY();
    }

    return stripe.contour_ <= RegionTop();
}

bool StdClusterWellLegalizer::StripeLegalizationTopDownCompact(Stripe &stripe) {
    stripe.cluster_list_.clear();
    stripe.contour_ = stripe.URY();
    stripe.used_height_ = 0;
    stripe.cluster_count_ = 0;
    stripe.front_cluster_ = nullptr;
    stripe.is_bottom_up_ = false;

    std::sort(stripe.block_list_.begin(),
              stripe.block_list_.end(),
              [](const Block *lhs, const Block *rhs) {
                  return (lhs->URY() > rhs->URY()) || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
              });
    for (auto &blk_ptr: stripe.block_list_) {
        if (blk_ptr->IsFixed()) continue;
        AppendBlockToColTopDownCompact(stripe, *blk_ptr);
    }

    for (auto &cluster: stripe.cluster_list_) {
        cluster.UpdateBlockLocY();
    }

    /*BOOST_LOG_TRIVIAL(info)   << "Reverse clustering: ";
    if (stripe.contour_ >= RegionLLY()) {
      BOOST_LOG_TRIVIAL(info)   << "success\n";
    } else {
      BOOST_LOG_TRIVIAL(info)   << "fail\n";
    }*/

    return stripe.contour_ >= RegionBottom();
}

bool StdClusterWellLegalizer::BlockClustering() {
    /****
     * Clustering blocks in each stripe
     * After clustering, close pack clusters from bottom to top
     ****/
    bool res = true;
    for (auto &col: col_list_) {
        bool is_success = true;
        for (auto &stripe: col.stripe_list_) {
            for (int i = 0; i < max_iter_; ++i) {
                is_success = StripeLegalizationBottomUp(stripe);
                if (!is_success) {
                    is_success = StripeLegalizationTopDown(stripe);
                }
            }
            res = res && is_success;

            // closely pack clusters from bottom to top
            stripe.contour_ = stripe.LLY();
            if (stripe.is_bottom_up_) {
                for (auto &cluster: stripe.cluster_list_) {
                    stripe.contour_ += cluster.Height();
                    cluster.SetLLY(stripe.contour_);
                    cluster.UpdateBlockLocY();
                    cluster.LegalizeCompactX();
                }
            } else {
                int sz = stripe.cluster_list_.size();
                for (int i = sz - 1; i >= 0; --i) {
                    auto &cluster = stripe.cluster_list_[i];
                    stripe.contour_ += cluster.Height();
                    cluster.SetLLY(stripe.contour_);
                    cluster.UpdateBlockLocY();
                    cluster.LegalizeCompactX();
                }
            }
        }
    }
    return res;
}

bool StdClusterWellLegalizer::BlockClusteringLoose() {
    /****
     * Clustering blocks in each stripe
     * After clustering, leave clusters as they are
     * ****/

    int step = 50;
    int count = 0;
    bool res = true;
    for (auto &col: col_list_) {
        bool is_success = true;
        for (auto &stripe: col.stripe_list_) {
            int i = 0;
            bool is_from_bottom = true;
            for (i = 0; i < max_iter_; ++i) {
                if (is_from_bottom) {
                    is_success = StripeLegalizationBottomUp(stripe);
                } else {
                    is_success = StripeLegalizationTopDown(stripe);
                }
                if (!is_success) {
                    is_success = TrialClusterLegalization(stripe);
                }
                is_from_bottom = !is_from_bottom;
                if (is_success) {
                    break;
                }
            }
            res = res && is_success;
            /*if (is_success) {
              BOOST_LOG_TRIVIAL(info)  <<"stripe legalization success, %d\n", i);
            } else {
              BOOST_LOG_TRIVIAL(info)  <<"stripe legalization fail, %d\n", i);
            }*/

            for (auto &cluster: stripe.cluster_list_) {
                cluster.UpdateBlockLocY();
                cluster.MinDisplacementLegalization();
                if (is_dump) {
                    if (count % step == 0) {
                        circuit_->GenMATLABTable("wlg_result_" + std::to_string(dump_count) + ".txt");
                        ++dump_count;
                    }
                    ++count;
                }
            }
            stripe.MinDisplacementAdjustment();
        }
    }

    return res;
}

bool StdClusterWellLegalizer::BlockClusteringCompact() {
    /****
     * Clustering blocks in each stripe in a compact way
     * After clustering, leave clusters as they are
     * ****/

    bool res = true;
    for (auto &col: col_list_) {
        bool is_success = true;
        for (auto &stripe: col.stripe_list_) {
            for (int i = 0; i < max_iter_; ++i) {
                is_success = StripeLegalizationBottomUpCompact(stripe);
                if (!is_success) {
                    is_success = StripeLegalizationTopDownCompact(stripe);
                }
            }
            res = res && is_success;

            for (auto &cluster: stripe.cluster_list_) {
                cluster.UpdateBlockLocY();
                cluster.LegalizeLooseX();
            }
        }
    }

    return res;
}

bool StdClusterWellLegalizer::TrialClusterLegalization(Stripe &stripe) {
    /****
     * Legalize the location of all clusters using extended Tetris legalization algorithm in columns where usage does not exceed capacity
     * Closely pack the column from bottom to top if its usage exceeds its capacity
     * ****/

    bool res = true;

    // sort clusters in each column based on the lower left corner
    std::vector<Cluster *> cluster_list;
    cluster_list.resize(stripe.cluster_count_, nullptr);
    for (int i = 0; i < stripe.cluster_count_; ++i) {
        cluster_list[i] = &stripe.cluster_list_[i];
    }

    //BOOST_LOG_TRIVIAL(info)   << "used height/RegionHeight(): " << col.used_height_ / (double) RegionHeight() << "\n";
    if (stripe.used_height_ <= RegionHeight()) {
        if (stripe.is_bottom_up_) {
            std::sort(cluster_list.begin(),
                      cluster_list.end(),
                      [](const Cluster *lhs, const Cluster *rhs) {
                          return (lhs->URY() > rhs->URY());
                      });
            int cluster_contour = stripe.URY();
            int res_y;
            int init_y;
            for (auto &cluster: cluster_list) {
                init_y = cluster->URY();
                res_y = std::min(cluster_contour, cluster->URY());
                cluster->SetURY(res_y);
                cluster_contour = cluster->LLY();
                cluster->ShiftBlockY(res_y - init_y);
            }
        } else {
            std::sort(cluster_list.begin(),
                      cluster_list.end(),
                      [](const Cluster *lhs, const Cluster *rhs) {
                          return (lhs->LLY() < rhs->LLY());
                      });
            int cluster_contour = stripe.LLY();
            int res_y;
            int init_y;
            for (auto &cluster: cluster_list) {
                init_y = cluster->LLY();
                res_y = std::max(cluster_contour, cluster->LLY());
                cluster->SetLLY(res_y);
                cluster_contour = cluster->URY();
                cluster->ShiftBlockY(res_y - init_y);
            }
        }
    } else {
        std::sort(cluster_list.begin(),
                  cluster_list.end(),
                  [](const Cluster *lhs, const Cluster *rhs) {
                      return (lhs->LLY() < rhs->LLY());
                  });
        int cluster_contour = RegionBottom();
        int res_y;
        int init_y;
        for (auto &cluster: cluster_list) {
            init_y = cluster->LLY();
            res_y = cluster_contour;
            cluster->SetLLY(res_y);
            cluster_contour += cluster->Height();
            cluster->ShiftBlockY(res_y - init_y);
        }
        res = false;
    }

    return res;
}

double StdClusterWellLegalizer::WireLengthCost(Cluster *cluster, int l, int r) {
    /****
     * Returns the wire-length cost of the small group from l-th element to r-th element in this cluster
     * "for each order, we keep the left and right boundaries of the group and evenly distribute the cells inside the group.
     * Since we have the Single-Segment Clustering technique to take care of the cell positions,
     * we do not pay much attention to the exact positions of the cells during Local Re-ordering."
     * from "An Efficient and Effective Detailed Placement Algorithm"
     * ****/

    auto &net_list = *NetList();
    std::set<Net *> net_involved;
    for (int i = l; i <= r; ++i) {
        auto *blk = cluster->blk_list_[i];
        for (auto &net_num: *blk->NetList()) {
            if (net_list[net_num].P() < 100) {
                net_involved.insert(&(net_list[net_num]));
            }
        }
    }

    double hpwl_x = 0;
    double hpwl_y = 0;
    for (auto &net: net_involved) {
        hpwl_x += net->WeightedHPWLX();
        hpwl_y += net->WeightedHPWLY();
    }

    return hpwl_x * circuit_->GridValueX() + hpwl_y * circuit_->GridValueY();
}

void StdClusterWellLegalizer::FindBestLocalOrder(std::vector<Block *> &res,
                                                 double &cost,
                                                 Cluster *cluster,
                                                 int cur,
                                                 int l,
                                                 int r,
                                                 int left_bound,
                                                 int right_bound,
                                                 int gap,
                                                 int range) {
    /****
    * Returns the best permutation in @param res
    * @param cost records the cost function associated with the best permutation
    * @param l is the left bound of the range
    * @param r is the right bound of the range
    * @param cluster points to the whole range, but we are only interested in the permutation of range [l,r]
    * ****/

    //BOOST_LOG_TRIVIAL(info)  <<"l : %d, r: %d\n", l, r);

    if (cur == r) {
        cluster->blk_list_[l]->SetLLX(left_bound);
        cluster->blk_list_[r]->SetURX(right_bound);

        int left_contour = left_bound + gap + cluster->blk_list_[l]->Width();
        for (int i = l + 1; i < r; ++i) {
            auto *blk = cluster->blk_list_[i];
            blk->SetLLX(left_contour);
            left_contour += blk->Width() + gap;
        }

        double tmp_cost = WireLengthCost(cluster, l, r);
        if (tmp_cost < cost) {
            cost = tmp_cost;
            for (int j = 0; j < range; ++j) {
                res[j] = cluster->blk_list_[l + j];
            }
        }
    } else {
        // Permutations made
        auto &blk_list = cluster->blk_list_;
        for (int i = cur; i <= r; ++i) {
            // Swapping done
            std::swap(blk_list[cur], blk_list[i]);

            // Recursion called
            FindBestLocalOrder(res, cost, cluster, cur + 1, l, r, left_bound, right_bound, gap, range);

            //backtrack
            std::swap(blk_list[cur], blk_list[i]);
        }
    }

}

void StdClusterWellLegalizer::LocalReorderInCluster(Cluster *cluster, int range) {
    /****
     * Enumerate all local permutations, @param range determines how big the local range is
     * ****/

    assert(range > 0);

    int sz = cluster->blk_list_.size();
    if (sz < 3) return;

    std::sort(cluster->blk_list_.begin(),
              cluster->blk_list_.end(),
              [](const Block *blk_ptr0, const Block *blk_ptr1) {
                  return blk_ptr0->LLX() < blk_ptr1->LLX();
              });

    int last_segment = sz - range;
    std::vector<Block *> res_local_order(range, nullptr);
    for (int l = 0; l <= last_segment; ++l) {
        int tot_blk_width = 0;
        for (int j = 0; j < range; ++j) {
            res_local_order[j] = cluster->blk_list_[l + j];
            tot_blk_width += res_local_order[j]->Width();
        }
        int r = l + range - 1;
        double best_cost = DBL_MAX;
        int left_bound = (int) cluster->blk_list_[l]->LLX();
        int right_bound = (int) cluster->blk_list_[r]->URX();
        int gap = (right_bound - left_bound - tot_blk_width) / (r - l);

        FindBestLocalOrder(res_local_order, best_cost, cluster, l, l, r, left_bound, right_bound, gap, range);
        for (int j = 0; j < range; ++j) {
            cluster->blk_list_[l + j] = res_local_order[j];
        }

        cluster->blk_list_[l]->SetLLX(left_bound);
        cluster->blk_list_[r]->SetURX(right_bound);
        int left_contour = left_bound + cluster->blk_list_[l]->Width() + gap;
        for (int i = l + 1; i < r; ++i) {
            auto *blk = cluster->blk_list_[i];
            blk->SetLLX(left_contour);
            left_contour += blk->Width() + gap;
        }
    }

}

void StdClusterWellLegalizer::LocalReorderAllClusters() {
    // sort cluster based on the lower left corner
    int tot_cluster_count = 0;
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            tot_cluster_count += stripe.cluster_list_.size();
        }
    }
    std::vector<Cluster *> cluster_ptr_list(tot_cluster_count, nullptr);
    int counter = 0;
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            for (auto &cluster: stripe.cluster_list_) {
                cluster_ptr_list[counter] = &cluster;
                ++counter;
            }
        }
    }
    std::sort(cluster_ptr_list.begin(),
              cluster_ptr_list.end(),
              [](const Cluster *lhs, const Cluster *rhs) {
                  return (lhs->LLY() < rhs->LLY()) || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
              });

    for (auto &cluster_ptr: cluster_ptr_list) {
        LocalReorderInCluster(cluster_ptr, 3);
    }
}

/*
void StdClusterWellLegalizer::SingleSegmentClusteringOptimization() {
  BOOST_LOG_TRIVIAL(info) << "Start single segment clustering\n";

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.cluster_list_) {
        int num_old_cluster = cluster.blk_list_.size();
        std::vector<BlockSegment> old_cluster(num_old_cluster);
        for (int i = 0; i < num_old_cluster; ++i) {
          old_cluster[i].blk_index.push_back(i);
          old_cluster[i].circuit_ = circuit_;
          old_cluster[i].cluster_ = &cluster;
          old_cluster[i].UpdateBoundList();
          old_cluster[i].SortBounds();
          old_cluster[i].UpdateLLX();
        }

        bool is_overlap = false;
        do {
          std::vector<BlockSegment> new_cluster;
          new_cluster.push_back(old_cluster[0]);
          int j = 0;
          while (j + 1 < num_old_cluster) {
            if (new_cluster.back().Overlap(old_cluster[j + 1])) {
              new_cluster.back().Merge(old_cluster[j + 1]);
            } else {
              new_cluster.push_back(old_cluster[j + 1]);
            }
            j += 1;
          }
          int new_count = new_cluster.size();
          num_old_cluster = new_count;
          for (int i = 0; i < new_count; ++i) {
            old_cluster[i].CopyFrom(new_cluster[i]);
          }

          is_overlap = false;
          for (int i = 0; i < new_count - 1; ++i) {
            if (old_cluster[i].IsNotOnLeft(old_cluster[i + 1])) {
              is_overlap = true;
              break;
            }
          }

          //BOOST_LOG_TRIVIAL(info)   << is_overlap << "\n";

        } while (is_overlap);

        for (int i = 0; i < num_old_cluster; ++i) {
          old_cluster[i].UpdateBlockLocation();
        }
      }
    }
  }

}
 */

void StdClusterWellLegalizer::UpdateClusterOrient() {
    for (auto &col: col_list_) {
        bool is_orient_N = is_first_row_orient_N_;
        for (auto &stripe: col.stripe_list_) {
            if (stripe.is_bottom_up_) {
                for (auto &cluster: stripe.cluster_list_) {
                    cluster.SetOrient(is_orient_N);
                    is_orient_N = !is_orient_N;
                }
            } else {
                int sz = stripe.cluster_list_.size();
                for (int i = sz - 1; i >= 0; --i) {
                    stripe.cluster_list_[i].SetOrient(is_orient_N);
                    is_orient_N = !is_orient_N;
                }
            }
        }
    }
}

void StdClusterWellLegalizer::InsertWellTap() {
    auto &tap_cell_list = circuit_->getDesign()->well_tap_list;
    tap_cell_list.clear();
    int tot_cluster_count = 0;
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            tot_cluster_count += stripe.cluster_list_.size();
        }
    }
    tap_cell_list.reserve(tot_cluster_count * 2);
    circuit_->getDesign()->tap_name_map.clear();

    int counter = 0;
    int tot_tap_cell_num = 0;
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            for (auto &cluster: stripe.cluster_list_) {
                //int tap_cell_num = std::ceil(cluster.Width() / (double) max_unplug_length_);
                int tap_cell_num = 2;
                tot_tap_cell_num += tap_cell_num;
                int step = cluster.Width();
                int tap_cell_loc = cluster.LLX() - well_tap_cell_->Width() / 2;
                for (int i = 0; i < tap_cell_num; ++i) {
                    std::string block_name = "__well_tap__" + std::to_string(counter++);
                    tap_cell_list.emplace_back();
                    auto &tap_cell = tap_cell_list.back();
                    tap_cell.SetPlacementStatus(PLACED);
                    tap_cell.SetType(circuit_->getTech()->WellTapCellRef()[0]);
                    int map_size = circuit_->getDesign()->tap_name_map.size();
                    auto ret =
                        circuit_->getDesign()->tap_name_map.insert(std::pair<std::string, int>(block_name, map_size));
                    auto *name_num_pair_ptr = &(*ret.first);
                    tap_cell.SetNameNumPair(name_num_pair_ptr);
                    cluster.InsertWellTapCell(tap_cell, tap_cell_loc);
                    tap_cell_loc += step;
                }
                cluster.LegalizeLooseX(space_to_well_tap_);
            }
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Inserting complete: " << tot_tap_cell_num << " well tap cell created\n";
}

void StdClusterWellLegalizer::ClearCachedData() {
    for (auto &block: circuit_->BlockListRef()) {
        block.SetOrient(N);
    }

    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            stripe.contour_ = stripe.LLY();
            stripe.used_height_ = 0;
            stripe.cluster_count_ = 0;
            stripe.cluster_list_.clear();
            stripe.front_cluster_ = nullptr;
        }
    }

    //cluster_list_.clear();
}

bool StdClusterWellLegalizer::WellLegalize() {
    bool is_success = true;
    Initialize();
    AssignBlockToColBasedOnWhiteSpace();
    is_success = BlockClusteringLoose();
    //BlockClusteringCompact();
    ReportHPWL();

    if (is_success) {
        BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                                << "Standard Cluster Well Legalization complete!\n"
                                << "\033[0m";
    } else {
        BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                                << "Standard Cluster Well Legalization fail!\n"
                                << "\033[0m";
    }

    return is_success;
}

bool StdClusterWellLegalizer::StartPlacement() {
    BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n"
                            << "Start Standard Cluster Well Legalization\n";

    double wall_time = get_wall_time();
    double cpu_time = get_cpu_time();

    /****---->****/
    bottom_ += 1;
    top_ -= 1;

    //circuit_->GenMATLABWellTable("lg", false);

    bool is_success = true;
    Initialize();
    AssignBlockToColBasedOnWhiteSpace();

    BOOST_LOG_TRIVIAL(info) << "Form block clustering\n";
    //BlockClustering();
    //is_success = BlockClusteringCompact();
    is_success = BlockClusteringLoose();
    ReportHPWL();
    //circuit_->GenMATLABWellTable("clu", false);
    //GenMatlabClusterTable("clu_result");

    BOOST_LOG_TRIVIAL(info) << "Flip cluster orientation\n";
    UpdateClusterOrient();
    ReportHPWL();
    //circuit_->GenMATLABWellTable("ori", false);
    //GenMatlabClusterTable("ori_result");

    BOOST_LOG_TRIVIAL(info) << "Perform local reordering\n";
    for (int i = 0; i < 6; ++i) {
        BOOST_LOG_TRIVIAL(info) << "reorder iteration: " << i;
        LocalReorderAllClusters();
        ReportHPWL();
        //BOOST_LOG_TRIVIAL(info) << "optimization: " << i;
        //SingleSegmentClusteringOptimization();
        //ReportHPWL();
    }
    //circuit_->GenMATLABWellTable("lop", false);
    //GenMatlabClusterTable("lop_result");

    BOOST_LOG_TRIVIAL(info) << "Insert well tap cells\n";
    InsertWellTap();
    ReportHPWL();
    //circuit_->GenMATLABWellTable("wtc", false);
    //GenMatlabClusterTable("wtc_result");

    bottom_ -= 1;
    top_ += 1;

    if (is_success) {
        BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                                << "Standard Cluster Well Legalization complete!\n"
                                << "\033[0m";
    } else {
        BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                                << "Standard Cluster Well Legalization fail!\n"
                                << "Please try lower density"
                                << "\033[0m";
    }
    /****<----****/

    wall_time = get_wall_time() - wall_time;
    cpu_time = get_cpu_time() - cpu_time;
    BOOST_LOG_TRIVIAL(info) << "(wall time: "
                            << wall_time << "s, cpu time: "
                            << cpu_time << "s)\n";

    ReportMemory();

    //ReportEffectiveSpaceUtilization();

    return is_success;
}

void StdClusterWellLegalizer::ReportEffectiveSpaceUtilization() {
    long int tot_std_blk_area = 0;
    int max_n_height = 0;
    int max_p_height = 0;
    for (auto &blk: circuit_->getDesign()->block_list) {
        BlockType *type = blk.TypePtr();
        if (type == circuit_->getTech()->io_dummy_blk_type_ptr_) continue;;
        if (type->WellPtr()->NHeight() > max_n_height) {
            max_n_height = type->WellPtr()->NHeight();
        }
        if (type->WellPtr()->PHeight() > max_p_height) {
            max_p_height = type->WellPtr()->PHeight();
        }
    }
    BlockTypeWell *well_tap_cell_well_info = circuit_->getTech()->WellTapCellRef()[0]->WellPtr();
    if (well_tap_cell_well_info->NHeight() > max_n_height) {
        max_n_height = well_tap_cell_well_info->NHeight();
    }
    if (well_tap_cell_well_info->PHeight() > max_p_height) {
        max_p_height = well_tap_cell_well_info->PHeight();
    }
    int max_height = max_n_height + max_p_height;

    long int tot_eff_blk_area = 0;
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            for (auto &cluster: stripe.cluster_list_) {
                long int eff_height = cluster.Height();
                long int tot_cell_width = 0;
                for (auto &blk_ptr: cluster.blk_list_) {
                    tot_cell_width += blk_ptr->Width();
                }
                tot_eff_blk_area += tot_cell_width * eff_height;
                tot_std_blk_area += tot_cell_width * max_height;
            }
        }
    }
    double factor = circuit_->getTech()->grid_value_x_ * circuit_->getTech()->grid_value_y_;
    BOOST_LOG_TRIVIAL(info) << "Total placement area: "
                            << ((long int) RegionWidth() * (long int) RegionHeight()) * factor
                            << " um^2\n";
    BOOST_LOG_TRIVIAL(info) << "Total block area: " << circuit_->TotBlkArea() * factor << " ("
                            << circuit_->TotBlkArea() / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";
    BOOST_LOG_TRIVIAL(info) << "Total effective block area: " << tot_eff_blk_area * factor << " ("
                            << tot_eff_blk_area / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";
    BOOST_LOG_TRIVIAL(info) << "Total standard block area (lower bound):" << tot_std_blk_area * factor << " ("
                            << tot_std_blk_area / (double) RegionWidth() / (double) RegionHeight() << ") um^2\n";

}

void StdClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
    std::string frame_file = name_of_file + "_outline.txt";
    GenMATLABTable(frame_file);

    std::string cluster_file = name_of_file + "_cluster.txt";
    std::ofstream ost(cluster_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + cluster_file);

    for (auto &col:col_list_) {
        for (auto &stripe: col.stripe_list_) {
            for (auto &cluster: stripe.cluster_list_) {
                ost << cluster.LLX() << "\t"
                    << cluster.URX() << "\t"
                    << cluster.URX() << "\t"
                    << cluster.LLX() << "\t"
                    << cluster.LLY() << "\t"
                    << cluster.LLY() << "\t"
                    << cluster.URY() << "\t"
                    << cluster.URY() << "\n";
            }
        }
    }
    ost.close();
}

void StdClusterWellLegalizer::GenMATLABWellTable(std::string const &name_of_file, int well_emit_mode) {
    circuit_->GenMATLABWellTable(name_of_file, false);

    std::string p_file = name_of_file + "_pwell.txt";
    std::ofstream ostp(p_file.c_str());
    DaliExpects(ostp.is_open(), "Cannot open output file: " + p_file);

    std::string n_file = name_of_file + "_nwell.txt";
    std::ofstream ostn(n_file.c_str());
    DaliExpects(ostn.is_open(), "Cannot open output file: " + n_file);

    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            std::vector<int> pn_edge_list;
            if (stripe.is_bottom_up_) {
                pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
                pn_edge_list.push_back(RegionBottom());
            } else {
                pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
                pn_edge_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
            }
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionTop());
            } else {
                pn_edge_list.push_back(RegionBottom());
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

    GenPPNP(name_of_file);
}

void StdClusterWellLegalizer::GenPPNP(const std::string &name_of_file) {
    std::string np_file = name_of_file + "_np.txt";
    std::ofstream ostnp(np_file.c_str());
    DaliExpects(ostnp.is_open(), "Cannot open output file: " + np_file);

    std::string pp_file = name_of_file + "_pp.txt";
    std::ofstream ostpp(pp_file.c_str());
    DaliExpects(ostpp.is_open(), "Cannot open output file: " + pp_file);

    int adjust_width = well_tap_cell_->Width();

    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            // draw NP and PP shapes from N/P-edge to N/P-edge
            std::vector<int> pn_edge_list;
            pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionBottom());
            } else {
                pn_edge_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
            }
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionTop());
            } else {
                pn_edge_list.push_back(RegionBottom());
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
                    ostnp << lx + adjust_width << "\t"
                          << ux - adjust_width << "\t"
                          << ux - adjust_width << "\t"
                          << lx + adjust_width << "\t"
                          << ly << "\t"
                          << ly << "\t"
                          << uy << "\t"
                          << uy << "\n";
                } else {
                    ostpp << lx + adjust_width << "\t"
                          << ux - adjust_width << "\t"
                          << ux - adjust_width << "\t"
                          << lx + adjust_width << "\t"
                          << ly << "\t"
                          << ly << "\t"
                          << uy << "\t"
                          << uy << "\n";
                }
                is_p_well_rect = !is_p_well_rect;
            }

            // draw NP and PP shapes from well-tap cell to well-tap cell
            std::vector<int> well_tap_top_bottom_list;
            well_tap_top_bottom_list.reserve(stripe.cluster_list_.size() + 2);
            if (stripe.is_bottom_up_) {
                well_tap_top_bottom_list.push_back(RegionBottom());
            } else {
                well_tap_top_bottom_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                if (stripe.is_bottom_up_) {
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
                } else {
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
                }
            }
            if (stripe.is_bottom_up_) {
                well_tap_top_bottom_list.push_back(RegionTop());
            } else {
                well_tap_top_bottom_list.push_back(RegionBottom());
                std::reverse(well_tap_top_bottom_list.begin(), well_tap_top_bottom_list.end());
            }
            DaliExpects(well_tap_top_bottom_list.size() % 2 == 0,
                        "Impossible to get an even number of well tap cell edges");

            is_p_well_rect = stripe.is_first_row_orient_N_;
            int lx0 = stripe.LLX();
            int ux0 = lx + adjust_width;
            int ux1 = stripe.URX();
            int lx1 = ux1 - adjust_width;
            rect_count = (int) well_tap_top_bottom_list.size() - 1;
            for (int i = 0; i < rect_count; i += 2) {
                ly = well_tap_top_bottom_list[i];
                uy = well_tap_top_bottom_list[i + 1];
                if (uy > ly) {
                    if (is_p_well_rect) {
                        ostpp << lx0 << "\t"
                              << ux0 << "\t"
                              << ux0 << "\t"
                              << lx0 << "\t"
                              << ly << "\t"
                              << ly << "\t"
                              << uy << "\t"
                              << uy << "\n";
                        ostpp << lx1 << "\t"
                              << ux1 << "\t"
                              << ux1 << "\t"
                              << lx1 << "\t"
                              << ly << "\t"
                              << ly << "\t"
                              << uy << "\t"
                              << uy << "\n";
                    } else {
                        ostnp << lx0 << "\t"
                              << ux0 << "\t"
                              << ux0 << "\t"
                              << lx0 << "\t"
                              << ly << "\t"
                              << ly << "\t"
                              << uy << "\t"
                              << uy << "\n";
                        ostnp << lx1 << "\t"
                              << ux1 << "\t"
                              << ux1 << "\t"
                              << lx1 << "\t"
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
    ostnp.close();
    ostpp.close();
}

/****
 * Emit three files:
 * 1. rect file including all N/P well rectangles
 * 2. rect file including all NP/PP rectangles
 * 3. cluster file including all cluster shapes
 *
 * @param well_mode
 * 0: emit both N-well and P-well
 * 1: emit N-well only
 * 2: emit P-well only
 *
 * @param enable_emitting_cluster
 * True: emit cluster file
 * Fale: do not emit this file
 * ****/
void StdClusterWellLegalizer::EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode, bool enable_emitting_cluster) {
    EmitPPNPRect(name_of_file + "ppnp.rect");
    EmitWellRect(name_of_file + "well.rect", well_emit_mode);
    if (enable_emitting_cluster) {
        EmitClusterRect(name_of_file + "_router.cluster");
    }
}

void StdClusterWellLegalizer::EmitPPNPRect(std::string const &name_of_file) {
    // emit rect file
    std::string NP_name = "nplus";
    std::string PP_name = "pplus";

    BOOST_LOG_TRIVIAL(info) << "Writing PP and NP rect file: " << name_of_file;

    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    double factor_x = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_x_;
    double factor_y = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_y_;

    ost << "bbox "
        << (int) (RegionLeft() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
        << (int) (RegionBottom() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << " "
        << (int) (RegionRight() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
        << (int) (RegionTop() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "\n";

    int adjust_width = well_tap_cell_->Width();

    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            // draw NP and PP shapes from N/P-edge to N/P-edge
            std::vector<int> pn_edge_list;
            pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionBottom());
            } else {
                pn_edge_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
            }
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionTop());
            } else {
                pn_edge_list.push_back(RegionBottom());
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
                    ost << "rect # " << NP_name << " ";
                } else {
                    ost << "rect # " << PP_name << " ";
                }
                ost << (lx + adjust_width) * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                    << ly * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\t"
                    << (ux - adjust_width) * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                    << uy * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\n";

                is_p_well_rect = !is_p_well_rect;
            }

            // draw NP and PP shapes from well-tap cell to well-tap cell
            std::vector<int> well_tap_top_bottom_list;
            well_tap_top_bottom_list.reserve(stripe.cluster_list_.size() + 2);
            if (stripe.is_bottom_up_) {
                well_tap_top_bottom_list.push_back(RegionBottom());
            } else {
                well_tap_top_bottom_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                if (stripe.is_bottom_up_) {
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
                } else {
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->URY());
                    well_tap_top_bottom_list.push_back(cluster.blk_list_[0]->LLY());
                }
            }
            if (stripe.is_bottom_up_) {
                well_tap_top_bottom_list.push_back(RegionTop());
            } else {
                well_tap_top_bottom_list.push_back(RegionBottom());
                std::reverse(well_tap_top_bottom_list.begin(), well_tap_top_bottom_list.end());
            }
            DaliExpects(well_tap_top_bottom_list.size() % 2 == 0,
                        "Impossible to get an even number of well tap cell edges");

            is_p_well_rect = stripe.is_first_row_orient_N_;
            int lx0 = stripe.LLX();
            int ux0 = lx + adjust_width;
            int ux1 = stripe.URX();
            int lx1 = ux1 - adjust_width;
            rect_count = (int) well_tap_top_bottom_list.size() - 1;
            for (int i = 0; i < rect_count; i += 2) {
                ly = well_tap_top_bottom_list[i];
                uy = well_tap_top_bottom_list[i + 1];
                if (uy > ly) {
                    if (!is_p_well_rect) {
                        ost << "rect # " << NP_name << " ";
                    } else {
                        ost << "rect # " << PP_name << " ";
                    }
                    ost << lx0 * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                        << ly * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\t"
                        << ux0 * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                        << uy * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\n";
                    if (!is_p_well_rect) {
                        ost << "rect # " << NP_name << " ";
                    } else {
                        ost << "rect # " << PP_name << " ";
                    }
                    ost << lx1 * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                        << ly * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\t"
                        << ux1 * factor_x + circuit_->getDesign()->die_area_offset_x_ << "\t"
                        << uy * factor_y + circuit_->getDesign()->die_area_offset_y_ << "\n";
                }
                is_p_well_rect = !is_p_well_rect;
            }
        }
    }
    ost.close();
    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void StdClusterWellLegalizer::EmitWellRect(std::string const &name_of_file, int well_emit_mode) {
    // emit rect file
    BOOST_LOG_TRIVIAL(info) << "Writing N/P-well rect file: " << name_of_file;

    switch (well_emit_mode) {
        case 0:BOOST_LOG_TRIVIAL(info) << "emit N/P wells, ";
            break;
        case 1:BOOST_LOG_TRIVIAL(info) << "emit N wells, ";
            break;
        case 2:BOOST_LOG_TRIVIAL(info) << "emit P wells, ";
            break;
        default:DaliExpects(false, "Invalid value for well_emit_mode in StdClusterWellLegalizer::EmitDEFWellFile()");
    }

    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    double factor_x = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_x_;
    double factor_y = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_y_;

    ost << "bbox "
        << (int) (RegionLeft() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
        << (int) (RegionBottom() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << " "
        << (int) (RegionRight() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
        << (int) (RegionTop() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "\n";
    for (auto &col: col_list_) {
        for (auto &stripe: col.stripe_list_) {
            std::vector<int> pn_edge_list;
            if (stripe.is_bottom_up_) {
                pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
                pn_edge_list.push_back(RegionBottom());
            } else {
                pn_edge_list.reserve(stripe.cluster_list_.size() + 2);
                pn_edge_list.push_back(RegionTop());
            }
            for (auto &cluster: stripe.cluster_list_) {
                pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
            }
            if (stripe.is_bottom_up_) {
                pn_edge_list.push_back(RegionTop());
            } else {
                pn_edge_list.push_back(RegionBottom());
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
                    is_p_well_rect = !is_p_well_rect;
                    if (well_emit_mode == 1) continue;
                    ost << "rect GND pwell ";
                } else {
                    is_p_well_rect = !is_p_well_rect;
                    if (well_emit_mode == 2) continue;
                    ost << "rect Vdd nwell ";
                }
                ost << (int) (lx * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
                    << (int) (ly * factor_y) + circuit_->getDesign()->die_area_offset_y_ << " "
                    << (int) (ux * factor_x) + circuit_->getDesign()->die_area_offset_x_ << " "
                    << (int) (uy * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "\n";
            }
        }
    }
    ost.close();
    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void StdClusterWellLegalizer::EmitClusterRect(std::string const &name_of_file) {
    /****
     * Emits a rect file for power routing
     * ****/

    BOOST_LOG_TRIVIAL(info) << "Writing cluster rect file: " << name_of_file << " for router, ";
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    double factor_x = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_x_;
    double factor_y = circuit_->getDesign()->def_distance_microns * circuit_->getTech()->grid_value_y_;
    //BOOST_LOG_TRIVIAL(info)   << "Actual x span: "
    //          << RegionLeft() * factor_x + +circuit_->getDesign()->die_area_offset_x_ << "  "
    //          << (col_list_.back().stripe_list_[0].URX() + well_spacing_) * factor_x + circuit_->getDesign()->die_area_offset_x_
    //          << "\n";
    for (int i = 0; i < tot_col_num_; ++i) {
        std::string column_name = "column" + std::to_string(i);
        ost << "STRIP " << column_name << "\n";

        auto &col = col_list_[i];
        for (auto &stripe: col.stripe_list_) {
            ost << "  "
                << (int) (stripe.LLX() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << "  "
                << (int) (stripe.URX() * factor_x) + circuit_->getDesign()->die_area_offset_x_ << "  ";
            if (stripe.is_first_row_orient_N_) {
                ost << "GND\n";
            } else {
                ost << "Vdd\n";
            }

            if (stripe.is_bottom_up_) {
                for (auto &cluster: stripe.cluster_list_) {
                    ost << "  "
                        << (int) (cluster.LLY() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "  "
                        << (int) (cluster.URY() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "\n";
                }
            } else {
                int sz = stripe.cluster_list_.size();
                for (int j = sz - 1; j >= 0; --j) {
                    auto &cluster = stripe.cluster_list_[j];
                    ost << "  "
                        << (int) (cluster.LLY() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "  "
                        << (int) (cluster.URY() * factor_y) + circuit_->getDesign()->die_area_offset_y_ << "\n";
                }
            }

            ost << "END " << column_name << "\n\n";
        }
    }
    ost.close();
    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void StdClusterWellLegalizer::PlotAvailSpace(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    ost << RegionLeft() << "\t"
        << RegionRight() << "\t"
        << RegionRight() << "\t"
        << RegionLeft() << "\t"
        << RegionBottom() << "\t"
        << RegionBottom() << "\t"
        << RegionTop() << "\t"
        << RegionTop() << "\t"
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
                << i * row_height_ + RegionBottom() << "\t"
                << i * row_height_ + RegionBottom() << "\t"
                << (i + 1) * row_height_ + RegionBottom() << "\t"
                << (i + 1) * row_height_ + RegionBottom() << "\t"
                << 1 << "\t"
                << 1 << "\t"
                << 1 << "\n";
        }
    }

    for (auto &block: *BlockList()) {
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

void StdClusterWellLegalizer::PlotAvailSpaceInCols(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    ost << RegionLeft() << "\t"
        << RegionRight() << "\t"
        << RegionRight() << "\t"
        << RegionLeft() << "\t"
        << RegionBottom() << "\t"
        << RegionBottom() << "\t"
        << RegionTop() << "\t"
        << RegionTop() << "\t"
        << 1 << "\t"
        << 1 << "\t"
        << 1 << "\n";
    for (auto &col: col_list_) {
        for (int i = 0; i < tot_num_rows_; ++i) {
            auto &row = col.white_space_[i];
            for (auto &seg: row) {
                ost << seg.lo << "\t"
                    << seg.hi << "\t"
                    << seg.hi << "\t"
                    << seg.lo << "\t"
                    << i * row_height_ + RegionBottom() << "\t"
                    << i * row_height_ + RegionBottom() << "\t"
                    << (i + 1) * row_height_ + RegionBottom() << "\t"
                    << (i + 1) * row_height_ + RegionBottom() << "\t"
                    << 0 << "\t"
                    << 1 << "\t"
                    << 1 << "\n";
            }
        }
    }

    for (auto &block: *BlockList()) {
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

void StdClusterWellLegalizer::PlotSimpleStripes(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    ost << RegionLeft() << "\t"
        << RegionRight() << "\t"
        << RegionRight() << "\t"
        << RegionLeft() << "\t"
        << RegionBottom() << "\t"
        << RegionBottom() << "\t"
        << RegionTop() << "\t"
        << RegionTop() << "\t"
        << 1 << "\t"
        << 1 << "\t"
        << 1 << "\n";
    for (auto &col: col_list_) {
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

    for (auto &block: *BlockList()) {
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

}
