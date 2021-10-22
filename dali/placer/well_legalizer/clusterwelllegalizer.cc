//
// Created by Yihang Yang on 2/28/20.
//

#include "clusterwelllegalizer.h"

#include <algorithm>

#include "dali/common/helper.h"

namespace dali {

ClusterWellLegalizer::ClusterWellLegalizer() : LGTetrisEx() {}

ClusterWellLegalizer::~ClusterWellLegalizer() {
    for (auto &cluster_ptr: cluster_set_) {
        delete cluster_ptr;
    }
    delete displace_viewer_;
}

void ClusterWellLegalizer::InitializeClusterLegalizer() {

    // data structure initialization
    InitLegalizer();
    InitLegalizerY();
    row_to_cluster_.resize(tot_num_rows_, nullptr);
    col_to_cluster_.resize(tot_num_cols_, nullptr);

    // parameters fetching
    auto &tech_params = circuit_->tech();
    auto &n_well_layer = tech_params.NwellLayer();
    auto &p_well_layer = tech_params.PwellLayer();
    double grid_value_x = circuit_->GridValueX();
    well_extension_x = std::ceil(n_well_layer.Overhang() / grid_value_x);
    //well_extension_y = std::ceil((n_well_layer->Overhang())/circuit_ptr_->GetGridValueY());
    //plug_width = std::ceil();
    BOOST_LOG_TRIVIAL(info) << "Well max plug distance:  um \n";
    BOOST_LOG_TRIVIAL(info) << "GridValueX: " << grid_value_x << " um\n";
    max_well_length = std::floor(n_well_layer.MaxPlugDist() / grid_value_x);

    // parameters setting
    /*if (max_well_length > RegionWidth()) {
      max_well_length = RegionWidth();
    } else if (max_well_length >= RegionWidth() / 2) {
      max_well_length = RegionWidth() / 2;
    } else if (max_well_length >= RegionWidth() / 3) {
      max_well_length = RegionWidth() / 3;
    }*/
    //max_well_length = RegionWidth();

    max_well_length = std::min(max_well_length, RegionWidth() / 7);
    max_well_length = std::min(max_well_length, circuit_->MinBlkWidth() * 7);

    new_cluster_cost_threshold = circuit_->MinBlkHeight();
}

void ClusterWellLegalizer::InitDisplaceViewer(int sz) {
    displace_viewer_ = new DisplaceViewer<int>;
    displace_viewer_->SetSize(sz);
}

void ClusterWellLegalizer::UploadClusterXY() {
    int counter = 0;
    for (auto &cluster: cluster_set_) {
        displace_viewer_->SetXY(counter++, cluster->LLX(), cluster->LLY());
    }
}

void ClusterWellLegalizer::UploadClusterUV() {
    int counter = 0;
    for (auto &cluster: cluster_set_) {
        displace_viewer_->SetXYFromDifference(counter++,
                                              cluster->LLX(),
                                              cluster->LLY());
    }
}

BlkCluster *ClusterWellLegalizer::CreateNewCluster() {
    auto *new_cluster =
        new BlkCluster(well_extension_x, well_extension_y, plug_width);
    cluster_set_.insert(new_cluster);
    return new_cluster;
}

void ClusterWellLegalizer::AddBlockToCluster(Block &block,
                                             BlkCluster *cluster) {
    /****
     * Append the @param block to the @param cluster
     * Update the row_to_cluster_
     * ****/
    cluster->AppendBlock(block);
    int lo_row = StartRow((int) block.LLY());
    int hi_row = EndRow((int) block.URY());
    lo_row = std::max(lo_row, 0);
    hi_row = std::min(tot_num_rows_ - 1, hi_row);

    for (int i = lo_row; i <= hi_row; ++i) {
        row_to_cluster_[i] = cluster;
    }
}

BlkCluster *ClusterWellLegalizer::FindClusterForBlock(Block &block) {
    /****
     * Returns the pointer to the cluster which gives the minimum cost.
     * Cost function is the displacement.
     * If there is no cluster nearby, create a new cluster, and return the new cluster.
     * If there are clusters nearby, but the displacement is too large, create a new cluster, and return the new cluster.
     * If there are clusters nearby, and one of them gives the smallest cost but the length of cluster is not too long, returns that cluster.
     * ****/
    int init_x = (int) block.LLX();
    int init_y = (int) block.LLY();
    int height = block.Height();

    int max_search_row = MaxRow(height);

    int search_start_row = std::max(0, LocToRow(init_y - 2 * height));
    int search_end_row =
        std::min(max_search_row, LocToRow(init_y + 3 * height));

    BlkCluster *res_cluster = nullptr;
    BlkCluster *pre_cluster = nullptr;
    BlkCluster *cur_cluster = nullptr;

    double min_cost = DBL_MAX;

    for (int i = search_start_row; i <= search_end_row; ++i) {
        cur_cluster = row_to_cluster_[i];
        if (cur_cluster == pre_cluster) {
            pre_cluster = cur_cluster;
            continue;
        }

        double cost_x = std::fabs(cur_cluster->InnerUX() - init_x);
        double cost_y = std::fabs(cur_cluster->CenterY() - block.Y());

        double tmp_cost = cost_x + cost_y;
        if (cur_cluster->Width() + block.Width() > max_well_length) {
            tmp_cost = DBL_MAX;
        }

        if (tmp_cost < min_cost) {
            min_cost = tmp_cost;
            res_cluster = cur_cluster;
        }

    }

    if (min_cost > new_cluster_cost_threshold) {
        res_cluster = CreateNewCluster();
    }

    return res_cluster;
}

void ClusterWellLegalizer::ClusterBlocks() {
    cluster_set_.clear();
    cluster_loc_list_.clear();

    std::vector<Block> &block_list = *BlockList();

    int sz = index_loc_list_.size();
    for (int i = 0; i < sz; ++i) {
        index_loc_list_[i].num = i;
        index_loc_list_[i].x = block_list[i].LLX();
        index_loc_list_[i].y = block_list[i].LLY();
    }
    std::sort(index_loc_list_.begin(), index_loc_list_.end());

    for (int i = 0; i < sz; ++i) {
        auto &block = block_list[index_loc_list_[i].num];

        if (block.IsFixed()) continue;

        BlkCluster *cluster = FindClusterForBlock(block);
        assert(cluster != nullptr);
        AddBlockToCluster(block, cluster);
    }
}

void ClusterWellLegalizer::UseSpaceLeft(int end_x, int lo_row, int hi_row) {
    assert(lo_row >= 0);
    assert(hi_row < tot_num_rows_);
    for (int i = lo_row; i <= hi_row; ++i) {
        block_contour_[i] = end_x;
    }
}

bool ClusterWellLegalizer::LegalizeClusterLeft() {
    bool is_successful = true;
    block_contour_.assign(block_contour_.size(), left_);

    int i = 0;
    for (auto &cluster_ptr: cluster_set_) {
        cluster_loc_list_[i].clus_ptr = cluster_ptr;
        cluster_loc_list_[i].x = cluster_ptr->LLX();
        cluster_loc_list_[i].y = cluster_ptr->LLY();
        //BOOST_LOG_TRIVIAL(info)   << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
        ++i;
    }
    std::sort(cluster_loc_list_.begin(), cluster_loc_list_.end());

    int height;
    int width;

    Value2D<int> res;
    bool is_current_loc_legal;
    bool is_legal_loc_found;
    int sz = cluster_loc_list_.size();

    for (i = 0; i < sz; ++i) {
        //BOOST_LOG_TRIVIAL(info)   << i << "\n";
        auto cluster = cluster_loc_list_[i].clus_ptr;
        assert(cluster != nullptr);

        res.x = int(std::round(cluster->LLX()));
        res.y = AlignLocToRowLoc(cluster->LLY());
        height = int(cluster->Height());
        width = int(cluster->Width());

        is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height);

        if (!is_current_loc_legal) {
            is_legal_loc_found = FindLocLeft(res, width, height);
            if (!is_legal_loc_found) {
                is_successful = false;
                //BOOST_LOG_TRIVIAL(info)   << res.x << "  " << res.y << "  " << cluster.Num() << " left\n";
                //break;
            }
        }

        cluster->SetLoc(res.x, res.y);

        UseSpaceLeft(cluster->URX(),
                     StartRow(cluster->LLY()),
                     EndRow(cluster->URY()));
    }

    return is_successful;
}

void ClusterWellLegalizer::UseSpaceRight(int end_x, int lo_row, int hi_row) {
    assert(lo_row >= 0);
    assert(hi_row < tot_num_rows_);
    for (int i = lo_row; i <= hi_row; ++i) {
        block_contour_[i] = end_x;
    }
}

bool ClusterWellLegalizer::LegalizeClusterRight() {
    bool is_successful = true;
    block_contour_.assign(block_contour_.size(), right_);

    int sz = cluster_loc_list_.size();
    int i = 0;
    for (auto &cluster_ptr: cluster_set_) {
        cluster_loc_list_[i].clus_ptr = cluster_ptr;
        cluster_loc_list_[i].x = cluster_ptr->URX();
        cluster_loc_list_[i].y = cluster_ptr->LLY();
        //BOOST_LOG_TRIVIAL(info)   << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
        ++i;
    }
    std::sort(cluster_loc_list_.begin(),
              cluster_loc_list_.end(),
              [](const CluPtrLocPair &lhs, const CluPtrLocPair &rhs) {
                  return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
              });

    int height;
    int width;

    Value2D<int> res;
    bool is_current_loc_legal;
    bool is_legal_loc_found;

    for (i = 0; i < sz; ++i) {
        //BOOST_LOG_TRIVIAL(info)   << i << "\n";
        auto cluster = cluster_loc_list_[i].clus_ptr;
        assert(cluster != nullptr);

        res.x = int(std::round(cluster->URX()));
        res.y = AlignLocToRowLoc(cluster->LLY());
        height = int(cluster->Height());
        width = int(cluster->Width());

        is_current_loc_legal = IsCurrentLocLegalRight(res, width, height);

        if (!is_current_loc_legal) {
            is_legal_loc_found = FindLocRight(res, width, height);
            if (!is_legal_loc_found) {
                is_successful = false;
                //BOOST_LOG_TRIVIAL(info)   << res.x << "  " << res.y << "  " << cluster.Num() << " left\n";
                //break;
            }
        }

        cluster->SetURX(res.x);
        cluster->SetLLY(res.y);

        UseSpaceRight(cluster->LLX(),
                      StartRow(cluster->LLY()),
                      EndRow(cluster->URY()));
    }

    return is_successful;
}

void ClusterWellLegalizer::UseSpaceBottom(int end_y, int lo_col, int hi_col) {
    assert(lo_col >= 0);
    assert(hi_col < tot_num_cols_);
    for (int i = lo_col; i <= hi_col; ++i) {
        block_contour_y_[i] = end_y;
    }
}

bool ClusterWellLegalizer::FindLocBottom(Value2D<int> &loc,
                                         int width,
                                         int height) {
    /****
     * Returns whether a legal location can be found, and put the final location to @params loc
     * ****/
    bool is_successful;

    int bottom_block_bound;
    int bottom_white_space_bound;

    int max_search_col;
    int search_start_col;
    int search_end_col;

    int best_y;
    int best_x;
    double min_cost;
    double cost_threshold = height;

    //bottom_block_bound = (int) std::round(loc.y - k_left_ * height);
    bottom_block_bound = bottom_;

    max_search_col = MaxCol(width);

    search_start_col = std::max(0, LocToCol(loc.x - 4 * width));
    search_end_col = std::min(max_search_col, LocToCol(loc.x + 5 * width));

    best_x = INT_MIN;
    best_y = INT_MIN;
    min_cost = DBL_MAX;

    for (int tmp_start_col = search_start_col; tmp_start_col <= search_end_col;
         ++tmp_start_col) {
        int tmp_end_col = tmp_start_col + width - 1;
        bottom_white_space_bound = bottom_;
        //bottom_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);

        int tmp_y = std::max(bottom_white_space_bound, bottom_block_bound);
        for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
            tmp_y = std::max(tmp_y, block_contour_y_[n]);
        }
        int tmp_x = ColToLoc(tmp_start_col);

        double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
            + CostLeftBottomBoundary(tmp_x, tmp_y);

        if (tmp_cost < min_cost) {
            best_x = tmp_x;
            best_y = tmp_y;
            min_cost = tmp_cost;
        }
    }

    int best_loc_x_legal = INT_MIN;
    int best_loc_y_legal = INT_MIN;
    bool legal_loc_found = false;
    double min_cost_legal = DBL_MAX;
    bool is_loc_legal =
        IsSpaceLegal(best_x,
                     best_x + width,
                     StartRow(best_y),
                     EndRow(best_y + height));

    if (!is_loc_legal) {
        int old_start_col = search_start_col;
        int old_end_col = search_end_col;
        int extended_range = cur_iter_ * width;
        search_start_col = std::max(0, search_start_col - extended_range);
        search_end_col =
            std::min(max_search_col, search_end_col + extended_range);
        for (int tmp_start_col = search_start_col;
             tmp_start_col < old_start_col; ++tmp_start_col) {
            int tmp_end_col = tmp_start_col + width - 1;
            //bottom_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);
            bottom_white_space_bound = bottom_;
            int tmp_y = std::max(bottom_white_space_bound, bottom_block_bound);

            for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
                tmp_y = std::max(tmp_y, block_contour_y_[n]);
            }

            int tmp_x = ColToLoc(tmp_start_col);
            //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

            double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
                + CostLeftBottomBoundary(tmp_x, tmp_y);
            if (tmp_cost < min_cost) {
                best_x = tmp_x;
                best_y = tmp_y;
                min_cost = tmp_cost;
            }

            is_loc_legal = IsSpaceLegal(tmp_x,
                                        tmp_x + width,
                                        StartRow(tmp_y),
                                        EndRow(tmp_y + height));

            if (is_loc_legal) {
                legal_loc_found = true;
                if (tmp_cost < min_cost_legal) {
                    best_loc_x_legal = tmp_x;
                    best_loc_y_legal = tmp_y;
                    min_cost_legal = tmp_cost;
                }
            }
        }
        for (int tmp_start_col = old_end_col; tmp_start_col < search_end_col;
             ++tmp_start_col) {
            int tmp_end_col = tmp_start_col + width - 1;
            //bottom_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);
            bottom_white_space_bound = bottom_;
            int tmp_y = std::max(bottom_white_space_bound, bottom_block_bound);

            for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
                tmp_y = std::max(tmp_y, block_contour_y_[n]);
            }

            int tmp_x = ColToLoc(tmp_start_col);
            //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

            double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
                + CostLeftBottomBoundary(tmp_x, tmp_y);
            if (tmp_cost < min_cost) {
                best_x = tmp_x;
                best_y = tmp_y;
                min_cost = tmp_cost;
            }

            is_loc_legal = IsSpaceLegal(tmp_x,
                                        tmp_x + width,
                                        StartRow(tmp_y),
                                        EndRow(tmp_y + height));

            if (is_loc_legal) {
                legal_loc_found = true;
                if (tmp_cost < min_cost_legal) {
                    best_loc_x_legal = tmp_x;
                    best_loc_y_legal = tmp_y;
                    min_cost_legal = tmp_cost;
                }
            }
        }

    }

    // if still cannot find a legal location, enter fail mode
    Value2D<int> tmp_loc(best_x, best_y);
    is_successful = IsCurrentLocLegalBottom(tmp_loc, width, height);
    if (!is_successful) {
        if (legal_loc_found) {
            best_x = best_loc_x_legal;
            best_y = best_loc_y_legal;
        }
    }

    loc.x = best_x;
    loc.y = best_y;

    return is_successful;
}

void ClusterWellLegalizer::FastShiftBottom(int failure_point) {
    std::vector<BlkCluster *>
        cluster_list(cluster_set_.begin(), cluster_set_.end());
    //BOOST_LOG_TRIVIAL(info)   << cluster_set.size() << "\n";
    int bounding_bottom;
    if (failure_point == 0) {
        BOOST_LOG_TRIVIAL(info)
            << "WARNING: unexpected case happens during legalization (failure point is 0)!\n";
    } else {
        int init_diff = cluster_loc_list_[failure_point - 1].y
            - cluster_loc_list_[failure_point].y;
        bounding_bottom = cluster_loc_list_[failure_point].clus_ptr->LLY();
        int bottom_new = cluster_loc_list_[failure_point - 1].clus_ptr->LLY();
        int sz = cluster_loc_list_.size();
        for (int i = failure_point; i < sz; ++i) {
            cluster_list[i]->IncreY(bottom_new - bounding_bottom + init_diff);
        }
    }
}

bool ClusterWellLegalizer::LegalizeClusterBottom() {
    bool is_successful = true;
    block_contour_y_.assign(tot_num_cols_, bottom_);

    // sort clusters based on their distance to the lower left corner
    int i = 0;
    for (auto &cluster_ptr: cluster_set_) {
        cluster_loc_list_[i].clus_ptr = cluster_ptr;
        cluster_loc_list_[i].x = cluster_ptr->LLX();
        cluster_loc_list_[i].y = cluster_ptr->LLY();
        ++i;
    }
    std::sort(cluster_loc_list_.begin(),
              cluster_loc_list_.end(),
              [](const CluPtrLocPair &lhs, const CluPtrLocPair &rhs) {
                  return (lhs.y < rhs.y) || (lhs.y == rhs.y && lhs.x < rhs.x);
              });

    int height;
    int width;

    Value2D<int> res;
    bool is_current_loc_legal;
    bool is_legal_loc_found;
    int sz = cluster_loc_list_.size();

    for (i = 0; i < sz; ++i) {
        //BOOST_LOG_TRIVIAL(info)   << i << "\n";
        auto cluster = cluster_loc_list_[i].clus_ptr;
        assert(cluster != nullptr);

        res.x = AlignLocToColLoc(cluster->LLX());
        res.y = AlignLocToRowLoc(cluster->LLY());
        height = int(cluster->Height());
        width = int(cluster->Width());

        //is_current_loc_legal = IsCurrentLocLegalBottom(res, width, height);
        //BOOST_LOG_TRIVIAL(info)   << res.x << "  " << res.y << "  " << is_current_loc_legal << "\n";
        is_current_loc_legal = false;
        if (!is_current_loc_legal) {
            is_legal_loc_found = FindLocBottom(res, width, height);
            //BOOST_LOG_TRIVIAL(info)   << res.x << "  " << res.y << "\n";
            if (!is_legal_loc_found) {
                is_successful = false;
                //break;
            }
        }

        cluster->SetLoc(res.x, res.y);
        UseSpaceBottom(cluster->URY(),
                       StartCol(cluster->LLX()),
                       EndCol(cluster->URX()));
    }

    /*if (!is_successful) {
      FastShiftBottom(i);
    }*/

    return is_successful;
}

void ClusterWellLegalizer::UseSpaceTop(int end_y, int lo_col, int hi_col) {
    assert(lo_col >= 0);
    assert(hi_col < tot_num_cols_);
    for (int i = lo_col; i <= hi_col; ++i) {
        block_contour_y_[i] = end_y;
    }
}

bool ClusterWellLegalizer::FindLocTop(Value2D<int> &loc,
                                      int width,
                                      int height) {
/****
   * Returns whether a legal location can be found, and put the final location to @params loc
   * ****/
    bool is_successful;

    int top_block_bound;
    int top_white_space_bound;

    int max_search_col;
    int search_start_col;
    int search_end_col;

    int best_x;
    int best_y;
    double min_cost;

    top_block_bound = (int) std::round(loc.y + k_left_ * height);
    //top_block_bound = loc.x;

    max_search_col = MaxCol(width);

    search_start_col = std::max(0, LocToCol(loc.x - 4 * width));
    search_end_col = std::min(max_search_col, LocToCol(loc.x + 5 * width));

    best_x = INT_MIN;
    best_y = INT_MIN;
    min_cost = DBL_MAX;

    for (int tmp_start_col = search_start_col; tmp_start_col <= search_end_col;
         ++tmp_start_col) {
        int tmp_end_col = tmp_start_col + width - 1;
        top_white_space_bound = top_;
        //top_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);

        int tmp_y = std::min(top_white_space_bound, top_block_bound);

        for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
            tmp_y = std::min(tmp_y, block_contour_y_[n]);
        }

        int tmp_x = ColToLoc(tmp_start_col);
        //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

        double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
            + CostRightTopBoundary(tmp_x, tmp_y);

        if (tmp_cost < min_cost) {
            best_x = tmp_x;
            best_y = tmp_y;
            min_cost = tmp_cost;
        }
    }

    int best_loc_x_legal = INT_MIN;
    int best_loc_y_legal = INT_MIN;
    bool legal_loc_found = false;
    double min_cost_legal = DBL_MAX;
    bool is_loc_legal =
        IsSpaceLegal(best_x,
                     best_x + width,
                     StartRow(best_y),
                     EndRow(best_y + height));

    if (!is_loc_legal) {
        int old_start_col = search_start_col;
        int old_end_col = search_end_col;
        int extended_range = cur_iter_ * width;
        search_start_col = std::max(0, search_start_col - extended_range);
        search_end_col =
            std::min(max_search_col, search_end_col + extended_range);
        for (int tmp_start_col = search_start_col;
             tmp_start_col < old_start_col; ++tmp_start_col) {
            int tmp_end_col = tmp_start_col + width - 1;
            //top_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);
            top_white_space_bound = top_;
            int tmp_y = std::min(top_white_space_bound, top_block_bound);

            for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
                tmp_y = std::min(tmp_y, block_contour_y_[n]);
            }

            int tmp_x = ColToLoc(tmp_start_col);
            //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

            double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
                + CostRightTopBoundary(tmp_x, tmp_y);

            if (tmp_cost < min_cost) {
                best_x = tmp_x;
                best_y = tmp_y;
                min_cost = tmp_cost;
            }

            Value2D<int> tmp_loc(tmp_x, tmp_y);
            is_loc_legal = IsCurrentLocLegalTop(tmp_loc, width, height);

            if (is_loc_legal) {
                legal_loc_found = true;
                if (tmp_cost < min_cost_legal) {
                    best_loc_x_legal = tmp_x;
                    best_loc_y_legal = tmp_y;
                    min_cost_legal = tmp_cost;
                }
            }
        }
        for (int tmp_start_col = old_end_col; tmp_start_col < search_end_col;
             ++tmp_start_col) {
            int tmp_end_col = tmp_start_col + width - 1;
            //top_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_col, tmp_end_col);
            top_white_space_bound = top_;
            int tmp_y = std::min(top_white_space_bound, top_block_bound);

            for (int n = tmp_start_col; n <= tmp_end_col; ++n) {
                tmp_y = std::min(tmp_y, block_contour_y_[n]);
            }

            int tmp_x = ColToLoc(tmp_start_col);
            //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

            double tmp_cost = CostInitDisplacement(tmp_x, tmp_y, loc.x, loc.y)
                + CostRightTopBoundary(tmp_x, tmp_y);

            if (tmp_cost < min_cost) {
                best_x = tmp_x;
                best_y = tmp_y;
                min_cost = tmp_cost;
            }

            Value2D<int> tmp_loc(tmp_x, tmp_y);
            is_loc_legal = IsCurrentLocLegalTop(tmp_loc, width, height);

            if (is_loc_legal) {
                legal_loc_found = true;
                if (tmp_cost < min_cost_legal) {
                    best_loc_x_legal = tmp_x;
                    best_loc_y_legal = tmp_y;
                    min_cost_legal = tmp_cost;
                }
            }
        }

    }

    // if still cannot find a legal location, enter fail mode
    Value2D<int> tmp_loc(best_x, best_y);
    is_successful = IsCurrentLocLegalTop(tmp_loc, width, height);
    if (!is_successful) {
        is_successful = legal_loc_found;
        if (is_successful) {
            best_x = best_loc_x_legal;
            best_y = best_loc_y_legal;
        }
    }

    loc.x = best_x;
    loc.y = best_y;

    return is_successful;
}

bool ClusterWellLegalizer::LegalizeClusterTop() {
    bool is_successful = true;
    block_contour_y_.assign(tot_num_cols_, top_);

    int i = 0;
    for (auto &cluster_ptr: cluster_set_) {
        cluster_loc_list_[i].clus_ptr = cluster_ptr;
        cluster_loc_list_[i].x = cluster_ptr->LLX();
        cluster_loc_list_[i].y = cluster_ptr->URY();
        //BOOST_LOG_TRIVIAL(info)   << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
        ++i;
    }
    std::sort(cluster_loc_list_.begin(),
              cluster_loc_list_.end(),
              [](const CluPtrLocPair &lhs, const CluPtrLocPair &rhs) {
                  return (lhs.y > rhs.y) || (lhs.y == rhs.y && lhs.x > rhs.x);
              });

    int height;
    int width;

    Value2D<int> res;
    bool is_current_loc_legal;
    bool is_legal_loc_found;
    int sz = cluster_loc_list_.size();

    for (i = 0; i < sz; ++i) {
        //BOOST_LOG_TRIVIAL(info)   << i << "\n";
        auto cluster = cluster_loc_list_[i].clus_ptr;
        assert(cluster != nullptr);

        res.x = AlignLocToColLoc(cluster->LLX());
        res.y = AlignLocToRowLoc(cluster->URY());
        height = int(cluster->Height());
        width = int(cluster->Width());

        //is_current_loc_legal = IsCurrentLocLegalTop(res, width, height);
        is_current_loc_legal = false;
        if (!is_current_loc_legal) {
            is_legal_loc_found = FindLocTop(res, width, height);
            if (!is_legal_loc_found) {
                is_successful = false;
                //BOOST_LOG_TRIVIAL(info)   << res.x << "  " << res.y << "  " << cluster.Num() << " left\n";
                //break;
            }
        }

        cluster->SetLLX(res.x);
        cluster->SetURY(res.y);
        //BOOST_LOG_TRIVIAL(info)   << cluster->LLX() - left_ << "  " << cluster->URX() - left_ << "  " << tot_num_cols_ << "\n";
        UseSpaceTop(cluster->LLY(),
                    StartCol(cluster->LLX()),
                    EndCol(cluster->URX()));
    }

    return is_successful;
}

bool ClusterWellLegalizer::LegalizeCluster(int iteration) {
    CluPtrLocPair tmp_clu_ptr_pair(nullptr, 0, 0);
    cluster_loc_list_.assign(cluster_set_.size(), tmp_clu_ptr_pair);

    long int tot_cluster_area = 0;
    for (auto &cluster: cluster_set_) {
        tot_cluster_area += cluster->Area();
    }

    BOOST_LOG_TRIVIAL(info) << "Total cluster area: " << tot_cluster_area
                            << "\n";
    BOOST_LOG_TRIVIAL(info) << "Total region area : "
                            << RegionHeight() * RegionWidth() << "\n";
    BOOST_LOG_TRIVIAL(info) << "            Ratio : "
                            << double(tot_cluster_area) / RegionWidth()
                                / RegionHeight()
                            << "\n";

    bool is_success = false;
    int counter = 0;
    for (cur_iter_ = 0; cur_iter_ < iteration; ++cur_iter_) {
        if (legalize_from_left_) {
            is_success = LegalizeClusterBottom();
            UpdateBlockLocation();
            //GenMatlabClusterTable("clb" + std::to_string(cur_iter_) + "_result");
            is_success = LegalizeClusterTop();
            UpdateBlockLocation();
            //GenMatlabClusterTable("clt" + std::to_string(cur_iter_) + "_result");
        } else {
            is_success = LegalizeClusterLeft();
            UpdateBlockLocation();
            //GenMatlabClusterTable("cll" + std::to_string(cur_iter_) + "_result");
            is_success = LegalizeClusterRight();
            UpdateBlockLocation();
            //GenMatlabClusterTable("clr" + std::to_string(cur_iter_) + "_result");
        }
        //BOOST_LOG_TRIVIAL(info)   << cur_iter_ << "-th iteration: " << is_success << "\n";
        ++counter;
        if (counter == 5) {
            legalize_from_left_ = !legalize_from_left_;
            counter = 0;
        }
        //++k_left_;
        //GenMatlabClusterTable("cl" + std::to_string(cur_iter_) + "_result");
        ReportHPWL();
        if (is_success) {
            break;
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Well legalization takes " << cur_iter_
                            << " iterations\n";
    if (!is_success) {
        BOOST_LOG_TRIVIAL(info) << "Legalization fails\n";
    }
    return is_success;
}

void ClusterWellLegalizer::UpdateBlockLocation() {
    for (auto &cluster_ptr: cluster_set_) {
        cluster_ptr->UpdateBlockLocation();
    }
}

void ClusterWellLegalizer::BlockGlobalSwap() {

}

void ClusterWellLegalizer::BlockVerticalSwap() {

}

double ClusterWellLegalizer::WireLengthCost(BlkCluster *cluster, int l, int r) {
    cluster->UpdateBlockLocation();
    std::set<Net *> net_involved;
    auto &net_list = *(NetList());
    for (int i = l; i <= r; ++i) {
        auto *blk = cluster->blk_ptr_list_[i];
        for (auto &net_num: blk->NetList()) {
            if (net_list[net_num].PinCnt() < 100) {
                net_involved.insert(&(net_list[net_num]));
            }
        }
    }

    double res = 0;
    for (auto &net: net_involved) {
        res += net->WeightedHPWL();
    }

    return res;
}

void ClusterWellLegalizer::FindBestPermutation(std::vector<Block *> &res,
                                               double &cost,
                                               BlkCluster *cluster,
                                               int l,
                                               int r,
                                               int range) {
    /****
     * Returns the best permutation in @param res
     * @param cost records the cost function associated with the best permutation
     * @param l is the left bound of the range
     * @param r is the right bound of the range
     * @param cluster points to the whole range, but we are only interested in the permutation of range [l,r]
     * ****/

    //BOOST_LOG_TRIVIAL(info)  <<"l : %d, r: %d\n", l, r);

    if (l == r) {
        /*for (auto &blk_ptr: cluster->blk_ptr_list_) {
          BOOST_LOG_TRIVIAL(info)   << blk_ptr->NameStr() << "  ";
        }
        BOOST_LOG_TRIVIAL(info)   << "\n";*/
        int tmp_l = r - range + 1;
        double tmp_cost = WireLengthCost(cluster, tmp_l, r);
        if (tmp_cost < cost) {
            cost = tmp_cost;
            for (int j = 0; j < range; ++j) {
                res[j] = cluster->blk_ptr_list_[tmp_l + j];
            }
        }
    } else {
        // Permutations made
        auto &blk_list = cluster->blk_ptr_list_;
        for (int i = l; i <= r; ++i) {

            // Swapping done
            std::swap(blk_list[l], blk_list[i]);

            // Recursion called
            FindBestPermutation(res, cost, cluster, l + 1, r, range);

            //backtrack
            std::swap(blk_list[l], blk_list[i]);
        }
    }

}

void ClusterWellLegalizer::LocalReorderInCluster(BlkCluster *cluster,
                                                 int range) {
    /****
     * Enumerate all local permutations, @param range determines how big the local range is
     * ****/

    assert(range > 0);

    int sz = cluster->size();
    if (sz < 3) return;

    int last_segment = sz - range;
    std::vector<Block *> best_permutation;
    best_permutation.assign(range, nullptr);
    for (int l = 0; l <= last_segment; ++l) {
        for (int j = 0; j < range; ++j) {
            best_permutation[j] = cluster->blk_ptr_list_[l + j];
        }
        int r = l + range - 1;
        double best_cost = DBL_MAX;
        FindBestPermutation(best_permutation, best_cost, cluster, l, r, range);
        for (int j = 0; j < range; ++j) {
            cluster->blk_ptr_list_[l + j] = best_permutation[j];
        }
        cluster->UpdateBlockLocation();
    }

}

void ClusterWellLegalizer::LocalReorderAllClusters() {
    int i = 0;
    for (auto &cluster_ptr: cluster_set_) {
        cluster_loc_list_[i].clus_ptr = cluster_ptr;
        cluster_loc_list_[i].x = cluster_ptr->LLX();
        cluster_loc_list_[i].y = cluster_ptr->LLY();
        //BOOST_LOG_TRIVIAL(info)   << i << "  " << cluster_ptr->LLX() << "  " << cluster_loc_list_[i].clus_ptr->LLX() << "\n";
        ++i;
    }
    std::sort(cluster_loc_list_.begin(), cluster_loc_list_.end());

    for (auto &pair: cluster_loc_list_) {
        LocalReorderInCluster(pair.clus_ptr);
    }
}

bool ClusterWellLegalizer::StartPlacement() {
    BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n"
                            << "Start Well Legalization\n";

    double wall_time = get_wall_time();
    double cpu_time = get_cpu_time();

    /****---->****/

    InitializeClusterLegalizer();
    ReportWellRule();
    ClusterBlocks();

    InitDisplaceViewer(cluster_set_.size());
    UploadClusterXY();

    UpdateBlockLocation();
    BOOST_LOG_TRIVIAL(info) << "HPWL right after clustering\n";
    ReportHPWL();
    GenMatlabClusterTable("clu_result");

    LegalizeCluster(max_iter_);
    UpdateBlockLocation();
    LocalReorderAllClusters();

    UploadClusterUV();
    displace_viewer_->SaveDisplacementVector("disp_result.txt");

    /****<----****/

    BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                            << "Cluster Well Legalization complete!\n"
                            << "\033[0m";
    ReportHPWL();

    wall_time = get_wall_time() - wall_time;
    cpu_time = get_cpu_time() - cpu_time;
    BOOST_LOG_TRIVIAL(info) << "(wall time: "
                            << wall_time << "s, cpu time: "
                            << cpu_time << "s)\n";

    ReportMemory();

    GenMatlabClusterTable("cl_result");

    return true;
}

void ClusterWellLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
    std::string frame_file = name_of_file + "_outline.txt";
    GenMATLABTable(frame_file);

    std::string cluster_file = name_of_file + "_cluster.txt";
    std::ofstream ost(cluster_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + cluster_file);

    for (auto &cluster: cluster_set_) {
        ost << cluster->LLX() << "\t"
            << cluster->URX() << "\t"
            << cluster->URX() << "\t"
            << cluster->LLX() << "\t"
            << cluster->LLY() << "\t"
            << cluster->LLY() << "\t"
            << cluster->URY() << "\t"
            << cluster->URY() << "\n";
    }
    ost.close();
}

void ClusterWellLegalizer::ReportWellRule() {
    BOOST_LOG_TRIVIAL(info)
        << "  Number of rows: " << tot_num_rows_ << "\n"
        << "  Number of blocks: " << index_loc_list_.size() << "\n"
        << "  Well Rules:\n"
        << "    WellSpacing: " << well_spacing_x << "\n"
        << "    MaxDist:     " << max_well_length << "\n"
        << "    (real):      "
        << std::floor(
            circuit_->tech().NwellLayer().MaxPlugDist()
                / circuit_->GridValueX()) << "\n"
        << "    WellWidth:   " << well_min_width << "\n"
        << "    OverhangX:   " << well_extension_x << "\n"
        << "    OverhangY:   " << well_extension_y << "\n";
}

}
