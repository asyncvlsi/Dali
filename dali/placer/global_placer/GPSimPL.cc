//
// Created by Yihang Yang on 8/4/2019.
//

#include "GPSimPL.h"

#include <cmath>

#include <algorithm>

#include <omp.h>

#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/placer/legalizer/LGTetrisEx.h"

namespace dali {

GPSimPL::GPSimPL() : Placer() {
    grid_bin_height = 0;
    grid_bin_width = 0;
    grid_cnt_y = 0;
    grid_cnt_x = 0;
    width_epsilon_ = 0;
    height_epsilon_ = 0;
}

GPSimPL::GPSimPL(double aspectRatio, double fillingRate) : Placer(aspectRatio,
                                                                  fillingRate) {
    grid_bin_height = 0;
    grid_bin_width = 0;
    grid_cnt_y = 0;
    grid_cnt_x = 0;
    width_epsilon_ = 0;
    height_epsilon_ = 0;
}

void GPSimPL::LoadConf(std::string const &config_file) {
    config_read(config_file.c_str());
    std::string variable;

    variable = "dali.GPSimPL.cg_iteration_";
    if (config_exists(variable.c_str()) == 1) {
        cg_iteration_ = config_get_int(variable.c_str());
    }
    variable = "dali.GPSimPL.cg_iteration_max_num_";
    if (config_exists(variable.c_str()) == 1) {
        cg_iteration_max_num_ = config_get_int(variable.c_str());
    }
    variable = "dali.GPSimPL.cg_stop_criterion_";
    if (config_exists(variable.c_str()) == 1) {
        cg_stop_criterion_ = config_get_real(variable.c_str());
    }
    variable = "dali.GPSimPL.net_model_update_stop_criterion_";
    if (config_exists(variable.c_str()) == 1) {
        net_model_update_stop_criterion_ = config_get_real(variable.c_str());
    }

    variable = "dali.GPSimPL.b2b_update_max_iteration_";
    if (config_exists(variable.c_str()) == 1) {
        b2b_update_max_iteration_ = config_get_int(variable.c_str());
    }
    variable = "dali.GPSimPL.max_iter_";
    if (config_exists(variable.c_str()) == 1) {
        max_iter_ = config_get_int(variable.c_str());
    }
    variable = "dali.GPSimPL.number_of_cell_in_bin_";
    if (config_exists(variable.c_str()) == 1) {
        number_of_cell_in_bin_ = config_get_int(variable.c_str());
    }
    variable = "dali.GPSimPL.net_ignore_threshold_";
    if (config_exists(variable.c_str()) == 1) {
        net_ignore_threshold_ = config_get_int(variable.c_str());
    }

    variable = "dali.GPSimPL.simpl_LAL_converge_criterion_";
    if (config_exists(variable.c_str()) == 1) {
        simpl_LAL_converge_criterion_ = config_get_real(variable.c_str());
    }
    variable = "dali.GPSimPL.polar_converge_criterion_";
    if (config_exists(variable.c_str()) == 1) {
        polar_converge_criterion_ = config_get_real(variable.c_str());
    }
    variable = "dali.GPSimPL.convergence_criteria_";
    if (config_exists(variable.c_str()) == 1) {
        convergence_criteria_ = config_get_int(variable.c_str());
    }
}

void GPSimPL::BlockLocRandomInit() {
    std::minstd_rand0 generator{1};
    int region_width = RegionWidth();
    int region_height = RegionHeight();
    std::uniform_real_distribution<double> distribution(0, 1);

    BOOST_LOG_TRIVIAL(info) << "HPWL before initialization: "
                            << circuit_->WeightedHPWL() << "\n";

    for (auto &block: circuit_->Blocks()) {
        if (block.IsMovable()) {
            block.SetCenterX(
                RegionLeft() + region_width * distribution(generator));
            block.SetCenterY(
                RegionBottom() + region_height * distribution(generator));
        }
    }
    BOOST_LOG_TRIVIAL(info)
        << "Block location uniform initialization complete\n";

    init_hpwl_x_ = circuit_->WeightedHPWLX();
    init_hpwl_y_ = circuit_->WeightedHPWLY();
    init_hpwl_ = init_hpwl_x_ + init_hpwl_y_;
    BOOST_LOG_TRIVIAL(info) << "HPWL after initialization: " << init_hpwl_
                            << "\n";

    if (is_dump) DumpResult("rand_init.txt");
}

void GPSimPL::BlockLocCenterInit() {
    double region_center_x = (RegionRight() + RegionLeft()) / 2.0;
    double region_center_y = (RegionTop() + RegionBottom()) / 2.0;
    int region_width = RegionWidth();
    int region_height = RegionHeight();

    std::minstd_rand0 generator{1};
    std::normal_distribution<double> distribution(0.0, 1.0 / 3);

    for (auto &block: circuit_->Blocks()) {
        if (!block.IsMovable()) continue;
        double x = region_center_x + region_width * distribution(generator);
        double y = region_center_y + region_height * distribution(generator);
        x = std::max(x, (double) RegionLeft());
        x = std::min(x, (double) RegionRight());
        y = std::max(y, (double) RegionBottom());
        y = std::min(y, (double) RegionTop());
        block.SetCenterX(x);
        block.SetCenterY(y);
    }
    BOOST_LOG_TRIVIAL(info)
        << "Block location gaussian initialization complete\n";
    init_hpwl_x_ = circuit_->WeightedHPWLX();
    init_hpwl_y_ = circuit_->WeightedHPWLY();
    init_hpwl_ = init_hpwl_x_ + init_hpwl_y_;
    BOOST_LOG_TRIVIAL(info) << "HPWL after initialization: " << init_hpwl_
                            << "\n";

    if (is_dump) DumpResult("rand_init.txt");
}

void GPSimPL::DriverLoadPairInit() {
    std::vector<BlkPairNets> &pair_list = circuit_->blk_pair_net_list_;
    std::unordered_map<std::pair<int, int>,
                       int,
                       boost::hash<std::pair<int, int>>>
        &pair_map = circuit_->blk_pair_map_;
    std::vector<Block> &block_list = circuit_->Blocks();
    int sz = block_list.size();

    pair_connect.resize(sz);
    for (auto &blk_pair: pair_list) {
        int num0 = blk_pair.blk_num0;
        int num1 = blk_pair.blk_num1;
        //BOOST_LOG_TRIVIAL(info)   << num0 << " " << num1 << "\n";
        pair_connect[num0].push_back(&blk_pair);
        pair_connect[num1].push_back(&blk_pair);
    }

    diagonal_pair.clear();
    diagonal_pair.reserve(sz);
    for (int i = 0; i < sz; ++i) {
        diagonal_pair.emplace_back(i, i);
        pair_connect[i].push_back(&(diagonal_pair[i]));
        std::sort(pair_connect[i].begin(),
                  pair_connect[i].end(),
                  [](const BlkPairNets *blk_pair0,
                     const BlkPairNets *blk_pair1) {
                      if (blk_pair0->blk_num0 == blk_pair1->blk_num0) {
                          return blk_pair0->blk_num1 < blk_pair1->blk_num1;
                      } else {
                          return blk_pair0->blk_num0 < blk_pair1->blk_num0;
                      }
                  });
    }

    std::vector<int> row_size(sz, 1);
    for (int i = 0; i < sz; ++i) {
        for (auto &blk_pair: pair_connect[i]) {
            int num0 = blk_pair->blk_num0;
            int num1 = blk_pair->blk_num1;
            if (num0 == num1) continue;
            if (block_list[num0].IsMovable() && block_list[num1].IsMovable()) {
                ++row_size[i];
            }
        }
    }
    Ax.reserve(row_size);
    SpMat_diag_x.resize(sz);
    for (int i = 0; i < sz; ++i) {
        for (auto &blk_pair: pair_connect[i]) {
            int num0 = blk_pair->blk_num0;
            int num1 = blk_pair->blk_num1;
            if (block_list[num0].IsMovable() && block_list[num1].IsMovable()) {
                int col;
                if (num0 == i) {
                    col = num1;
                } else {
                    col = num0;
                }
                Ax.insertBackUncompressed(i, col) = 0;
            }
        }
        std::pair<int, int> key = std::make_pair(0, 0);
        for (SpMat::InnerIterator it(Ax, i); it; ++it) {
            int row = it.row();
            int col = it.col();
            if (row == col) {
                SpMat_diag_x[i] = it;
                continue;
            }
            key.first = std::max(row, col);
            key.second = std::min(row, col);
            if (pair_map.find(key) == pair_map.end()) {
                BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
                DaliExpects(false,
                            "Cannot find block pair in the database, something wrong happens\n");
            }
            int pair_index = pair_map[key];
            if (pair_list[pair_index].blk_num0 == row) {
                pair_list[pair_index].it01x = it;
            } else {
                pair_list[pair_index].it10x = it;
            }
        }
    }

    Ay.reserve(row_size);
    SpMat_diag_y.resize(sz);
    for (int i = 0; i < sz; ++i) {
        for (auto &blk_pair: pair_connect[i]) {
            int num0 = blk_pair->blk_num0;
            int num1 = blk_pair->blk_num1;
            if (block_list[num0].IsMovable() && block_list[num1].IsMovable()) {
                int col;
                if (num0 == i) {
                    col = num1;
                } else {
                    col = num0;
                }
                Ay.insertBackUncompressed(i, col) = 0;
            }
        }
        std::pair<int, int> key = std::make_pair(0, 0);
        for (SpMat::InnerIterator it(Ay, i); it; ++it) {
            int row = it.row();
            int col = it.col();
            if (row == col) {
                SpMat_diag_y[i] = it;
                continue;
            }
            key.first = std::max(row, col);
            key.second = std::min(row, col);
            if (pair_map.find(key) == pair_map.end()) {
                BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
                DaliExpects(false,
                            "Cannot find block pair in the database, something wrong happens\n");
            }
            int pair_index = pair_map[key];
            if (pair_list[pair_index].blk_num0 == row) {
                pair_list[pair_index].it01y = it;
            } else {
                pair_list[pair_index].it10y = it;
            }
        }
    }

//  BOOST_LOG_TRIVIAL(info)   << SpMat_diag_x[0].value() << "\n";
//  SpMat_diag_x[0].valueRef() = 1;
//  BOOST_LOG_TRIVIAL(info)   << SpMat_diag_x[0].value() << "\n";
//  exit(1);
}

void GPSimPL::CGInit() {
    SetEpsilon(); // set a small value for net weight dividend to avoid divergence
    lower_bound_hpwlx_.clear();
    lower_bound_hpwly_.clear();
    lower_bound_hpwl_.clear();

    int sz = Blocks().size();
    ADx.resize(sz);
    ADy.resize(sz);
    Ax_row_size.assign(sz, 0);
    Ax_row_size.assign(sz, 0);
    vx.resize(sz);
    vy.resize(sz);
    bx.resize(sz);
    by.resize(sz);
    Ax.resize(sz, sz);
    Ay.resize(sz, sz);
    x_anchor.resize(sz);
    y_anchor.resize(sz);
    x_anchor_weight.resize(sz);
    y_anchor_weight.resize(sz);

    cgx.setMaxIterations(cg_iteration_);
    cgx.setTolerance(cg_tolerance_);
    cgy.setMaxIterations(cg_iteration_);
    cgy.setTolerance(cg_tolerance_);

    int coefficient_size = 0;
    int net_sz = 0;
    for (auto &net: *NetList()) {
        net_sz = net.PinCnt();
        // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero entries in the matrix
        if (net_sz > 1) coefficient_size += ((net_sz - 2) * 2 + 1) * 4;
    }
    // this is to reserve space for anchor, because each block may need an anchor
    coefficient_size += sz;

    // this is to reserve space for anchor in the center of the placement region
    coefficient_size += sz;
    coefficientsx.reserve(coefficient_size);
    Ax.reserve(coefficient_size);
    coefficientsy.reserve(coefficient_size);
}

void GPSimPL::UpdateMaxMinX() {
    std::vector<Net> &net_list = circuit_->NetListRef();
    int sz = net_list.size();
#pragma omp parallel for
    for (int i = 0; i < sz; ++i) {
        net_list[i].UpdateMaxMinIdX();
    }
}

void GPSimPL::UpdateMaxMinY() {
    std::vector<Net> &net_list = circuit_->NetListRef();
    int sz = net_list.size();
#pragma omp parallel for
    for (int i = 0; i < sz; ++i) {
        net_list[i].UpdateMaxMinIdY();
    }
}

void GPSimPL::BuildProblemB2BX() {
    double wall_time = get_wall_time();

    std::vector<Net> &net_list = *NetList();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsx.capacity();
    coefficientsx.resize(0);

    int sz = bx.size();
    for (int i = 0; i < sz; ++i) {
        bx[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    double pin_loc;
    int blk_num;
    bool is_movable;
    double offset;
    double offset_diff;

    int max_pin_index;
    int blk_num_max;
    double pin_loc_max;
    bool is_movable_max;
    double offset_max;

    int min_pin_index;
    int blk_num_min;
    double pin_loc_min;
    bool is_movable_min;
    double offset_min;

    for (auto &net: net_list) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();
        net.UpdateMaxMinIdX();
        max_pin_index = net.MaxBlkPinIdX();
        min_pin_index = net.MinBlkPinIdX();

        blk_num_max = net.BlockPins()[max_pin_index].BlkId();
        pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
        is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
        offset_max = net.BlockPins()[max_pin_index].OffsetX();

        blk_num_min = net.BlockPins()[min_pin_index].BlkId();
        pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
        is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
        offset_min = net.BlockPins()[min_pin_index].OffsetX();

        for (auto &pair: net.BlockPins()) {
            blk_num = pair.BlkId();
            pin_loc = pair.AbsX();
            is_movable = pair.BlkPtr()->IsMovable();
            offset = pair.OffsetX();

            if (blk_num != blk_num_max) {
                double distance = std::fabs(pin_loc - pin_loc_max);
                weight = inv_p / (distance + width_epsilon_);
                //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
                //weight *= weight_adjust;
                if (!is_movable && is_movable_max) {
                    bx[blk_num_max] += (pin_loc - offset_max) * weight;
                    coefficientsx.emplace_back(blk_num_max,
                                               blk_num_max,
                                               weight);
                } else if (is_movable && !is_movable_max) {
                    bx[blk_num] += (pin_loc_max - offset) * weight;
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && is_movable_max) {
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                    coefficientsx.emplace_back(blk_num_max,
                                               blk_num_max,
                                               weight);
                    coefficientsx.emplace_back(blk_num, blk_num_max, -weight);
                    coefficientsx.emplace_back(blk_num_max, blk_num, -weight);
                    offset_diff = (offset_max - offset) * weight;
                    bx[blk_num] += offset_diff;
                    bx[blk_num_max] -= offset_diff;
                }
            }

            if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
                double distance = std::fabs(pin_loc - pin_loc_min);
                weight = inv_p / (distance + width_epsilon_);
                //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
                //weight *= weight_adjust;
                if (!is_movable && is_movable_min) {
                    bx[blk_num_min] += (pin_loc - offset_min) * weight;
                    coefficientsx.emplace_back(blk_num_min,
                                               blk_num_min,
                                               weight);
                } else if (is_movable && !is_movable_min) {
                    bx[blk_num] += (pin_loc_min - offset) * weight;
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && is_movable_min) {
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                    coefficientsx.emplace_back(blk_num_min,
                                               blk_num_min,
                                               weight);
                    coefficientsx.emplace_back(blk_num, blk_num_min, -weight);
                    coefficientsx.emplace_back(blk_num_min, blk_num, -weight);
                    offset_diff = (offset_min - offset) * weight;
                    bx[blk_num] += offset_diff;
                    bx[blk_num_min] -= offset_diff;
                }
            }
        }
    }

    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) {
            coefficientsx.emplace_back(i, i, 1);
            bx[i] = block_list[i].LLX();
        } else {
            if (block_list[i].LLX() < RegionLeft()
                || block_list[i].URX() > RegionRight()) {
                coefficientsx.emplace_back(i, i, center_weight);
                bx[i] += weight_center_x;
            }
        }
    }

    if (coefficients_capacity != coefficientsx.capacity()) {
        BOOST_LOG_TRIVIAL(warning)
            << "WARNING: coefficients capacity changed!\n"
            << "\told capacity: " << coefficients_capacity << "\n"
            << "\tnew capacity: " << coefficientsx.size() << "\n";
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_x += wall_time;

//  static int count = 0;
//  count += 1;
//  if (count == 100) {
//    bool s = saveMarket(Ax, "sparse_matrices");
//    if (s) {
//      BOOST_LOG_TRIVIAL(info)   << "Sparse matrix saved successfully\n";
//    }
//  }
}

void GPSimPL::BuildProblemB2BY() {
    double wall_time = get_wall_time();

    UpdateMaxMinY();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsy.capacity();
    coefficientsy.resize(0);

    int sz = by.size();
    for (int i = 0; i < sz; ++i) {
        by[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    double pin_loc;
    int blk_num;
    bool is_movable;
    double offset;
    double offset_diff;

    int max_pin_index;
    int blk_num_max;
    double pin_loc_max;
    bool is_movable_max;
    double offset_max;

    int min_pin_index;
    int blk_num_min;
    double pin_loc_min;
    bool is_movable_min;
    double offset_min;

    for (auto &net: *NetList()) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();
        //net.UpdateMaxMinIndexY();
        max_pin_index = net.MaxBlkPinIdY();
        min_pin_index = net.MinBlkPinIdY();

        blk_num_max = net.BlockPins()[max_pin_index].BlkId();
        pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
        is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
        offset_max = net.BlockPins()[max_pin_index].OffsetY();

        blk_num_min = net.BlockPins()[min_pin_index].BlkId();
        pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
        is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
        offset_min = net.BlockPins()[min_pin_index].OffsetY();

        for (auto &pair: net.BlockPins()) {
            blk_num = pair.BlkId();
            pin_loc = pair.AbsY();
            is_movable = pair.BlkPtr()->IsMovable();
            offset = pair.OffsetY();

            if (blk_num != blk_num_max) {
                double distance = std::fabs(pin_loc - pin_loc_max);
                weight = inv_p / (distance + height_epsilon_);
                //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
                //weight *= weight_adjust;
                if (!is_movable && is_movable_max) {
                    by[blk_num_max] += (pin_loc - offset_max) * weight;
                    coefficientsy.emplace_back(blk_num_max,
                                               blk_num_max,
                                               weight);
                } else if (is_movable && !is_movable_max) {
                    by[blk_num] += (pin_loc_max - offset) * weight;
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && is_movable_max) {
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                    coefficientsy.emplace_back(blk_num_max,
                                               blk_num_max,
                                               weight);
                    coefficientsy.emplace_back(blk_num, blk_num_max, -weight);
                    coefficientsy.emplace_back(blk_num_max, blk_num, -weight);
                    offset_diff = (offset_max - offset) * weight;
                    by[blk_num] += offset_diff;
                    by[blk_num_max] -= offset_diff;
                }
            }

            if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
                double distance = std::fabs(pin_loc - pin_loc_min);
                weight = inv_p / (distance + height_epsilon_);
                //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
                //weight *= weight_adjust;
                if (!is_movable && is_movable_min) {
                    by[blk_num_min] += (pin_loc - offset_min) * weight;
                    coefficientsy.emplace_back(blk_num_min,
                                               blk_num_min,
                                               weight);
                } else if (is_movable && !is_movable_min) {
                    by[blk_num] += (pin_loc_min - offset) * weight;
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && is_movable_min) {
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                    coefficientsy.emplace_back(blk_num_min,
                                               blk_num_min,
                                               weight);
                    coefficientsy.emplace_back(blk_num, blk_num_min, -weight);
                    coefficientsy.emplace_back(blk_num_min, blk_num, -weight);
                    offset_diff = (offset_min - offset) * weight;
                    by[blk_num] += offset_diff;
                    by[blk_num_min] -= offset_diff;
                }
            }

        }
    }
    for (int i = 0; i < sz;
         ++i) { // add the diagonal non-zero element for fixed blocks
        if (block_list[i].IsFixed()) {
            coefficientsy.emplace_back(i, i, 1);
            by[i] = block_list[i].LLY();
        } else {
            if (block_list[i].LLY() < RegionBottom()
                || block_list[i].URY() > RegionTop()) {
                coefficientsy.emplace_back(i, i, center_weight);
                by[i] += weight_center_y;
            }
        }
    }

    if (coefficients_capacity != coefficientsy.capacity()) {
        BOOST_LOG_TRIVIAL(warning)
            << "WARNING: coefficients capacity changed!\n"
            << "\told capacity: " << coefficients_capacity << "\n"
            << "\tnew capacity: " << coefficientsy.size() << "\n";
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_y += wall_time;
}

void GPSimPL::BuildProblemStarModelX() {
    double wall_time = get_wall_time();

    std::vector<Net> &net_list = *NetList();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsx.capacity();
    coefficientsx.resize(0);

    int sz = bx.size();
    for (int i = 0; i < sz; ++i) {
        bx[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    double pin_loc;
    int blk_num;
    bool is_movable;
    double offset_diff;

    int driver_blk_num;
    double driver_pin_loc;
    bool driver_is_movable;
    double driver_offset;

    for (auto &net: net_list) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();

        // assuming the 0-th pin in the net is the driver pin
        driver_blk_num = net.BlockPins()[0].BlkId();
        driver_pin_loc = net.BlockPins()[0].AbsX();
        driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
        driver_offset = net.BlockPins()[0].OffsetX();

        for (auto &pair: net.BlockPins()) {
            blk_num = pair.BlkId();
            pin_loc = pair.AbsX();
            is_movable = pair.BlkPtr()->IsMovable();

            if (blk_num != driver_blk_num) {
                double distance = std::fabs(pin_loc - driver_pin_loc);
                weight_adjust = base_factor
                    + adjust_factor * (1 - exp(-distance / decay_length));
                weight = inv_p / (distance + width_epsilon_) * weight_adjust;
                //weight = inv_p / (distance + width_epsilon_);
                if (!is_movable && driver_is_movable) {
                    bx[0] += (pin_loc - driver_offset) * weight;
                    coefficientsx.emplace_back(driver_blk_num,
                                               driver_blk_num,
                                               weight);
                } else if (is_movable && !driver_is_movable) {
                    bx[blk_num] += (driver_pin_loc - driver_offset) * weight;
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && driver_is_movable) {
                    coefficientsx.emplace_back(blk_num, blk_num, weight);
                    coefficientsx.emplace_back(driver_blk_num,
                                               driver_blk_num,
                                               weight);
                    coefficientsx.emplace_back(blk_num,
                                               driver_blk_num,
                                               -weight);
                    coefficientsx.emplace_back(driver_blk_num,
                                               blk_num,
                                               -weight);
                    offset_diff = (driver_offset - pair.OffsetX()) * weight;
                    bx[blk_num] += offset_diff;
                    bx[driver_blk_num] -= offset_diff;
                }
            }
        }
    }

    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) {
            coefficientsx.emplace_back(i, i, 1);
            bx[i] = block_list[i].LLX();
        } else {
            if (block_list[i].LLX() < RegionLeft()
                || block_list[i].URX() > RegionRight()) {
                coefficientsx.emplace_back(i, i, center_weight);
                bx[i] += weight_center_x;
            }
        }
    }

    if (coefficients_capacity != coefficientsx.capacity()) {
        BOOST_LOG_TRIVIAL(warning)
            << "WARNING: coefficients capacity changed!\n"
            << "\told capacity: " << coefficients_capacity << "\n"
            << "\tnew capacity: " << coefficientsx.size() << "\n";
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_x += wall_time;
}

void GPSimPL::BuildProblemStarModelY() {
    double wall_time = get_wall_time();

    std::vector<Net> &net_list = *NetList();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsy.capacity();
    coefficientsy.resize(0);

    int sz = by.size();
    for (int i = 0; i < sz; ++i) {
        by[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    double pin_loc;
    int blk_num;
    bool is_movable;
    double offset_diff;

    int driver_blk_num;
    double driver_pin_loc;
    bool driver_is_movable;
    double driver_offset;

    for (auto &net: net_list) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();

        // assuming the 0-th pin in the net is the driver pin
        driver_blk_num = net.BlockPins()[0].BlkId();
        driver_pin_loc = net.BlockPins()[0].AbsY();
        driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
        driver_offset = net.BlockPins()[0].OffsetY();

        for (auto &pair: net.BlockPins()) {
            blk_num = pair.BlkId();
            pin_loc = pair.AbsY();
            is_movable = pair.BlkPtr()->IsMovable();

            if (blk_num != driver_blk_num) {
                double distance = std::fabs(pin_loc - driver_pin_loc);
                weight_adjust = base_factor
                    + adjust_factor * (1 - exp(-distance / decay_length));
                weight = inv_p / (distance + height_epsilon_) * weight_adjust;
                //weight = inv_p / (distance + height_epsilon_);
                if (!is_movable && driver_is_movable) {
                    by[0] += (pin_loc - driver_offset) * weight;
                    coefficientsy.emplace_back(driver_blk_num,
                                               driver_blk_num,
                                               weight);
                } else if (is_movable && !driver_is_movable) {
                    by[blk_num] += (driver_pin_loc - driver_offset) * weight;
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                } else if (is_movable && driver_is_movable) {
                    coefficientsy.emplace_back(blk_num, blk_num, weight);
                    coefficientsy.emplace_back(driver_blk_num,
                                               driver_blk_num,
                                               weight);
                    coefficientsy.emplace_back(blk_num,
                                               driver_blk_num,
                                               -weight);
                    coefficientsy.emplace_back(driver_blk_num,
                                               blk_num,
                                               -weight);
                    offset_diff = (driver_offset - pair.OffsetY()) * weight;
                    by[blk_num] += offset_diff;
                    by[driver_blk_num] -= offset_diff;
                }
            }
        }
    }

    for (int i = 0; i < sz;
         ++i) { // add the diagonal non-zero element for fixed blocks
        if (block_list[i].IsFixed()) {
            coefficientsy.emplace_back(i, i, 1);
            by[i] = block_list[i].LLY();
        } else {
            if (block_list[i].LLY() < RegionBottom()
                || block_list[i].URY() > RegionTop()) {
                coefficientsy.emplace_back(i, i, center_weight);
                by[i] += weight_center_y;
            }
        }
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_y += wall_time;
}

void GPSimPL::BuildProblemHPWLX() {
    double wall_time = get_wall_time();

    std::vector<Net> &net_list = *NetList();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsx.capacity();
    coefficientsx.resize(0);

    int sz = bx.size();
    for (int i = 0; i < sz; ++i) {
        bx[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    int max_pin_index;
    int blk_num_max;
    double pin_loc_max;
    bool is_movable_max;
    double offset_max;

    int min_pin_index;
    int blk_num_min;
    double pin_loc_min;
    bool is_movable_min;
    double offset_min;

    for (auto &net: net_list) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();
        net.UpdateMaxMinIdX();
        max_pin_index = net.MaxBlkPinIdX();
        min_pin_index = net.MinBlkPinIdX();

        blk_num_max = net.BlockPins()[max_pin_index].BlkId();
        pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
        is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
        offset_max = net.BlockPins()[max_pin_index].OffsetX();

        blk_num_min = net.BlockPins()[min_pin_index].BlkId();
        pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
        is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
        offset_min = net.BlockPins()[min_pin_index].OffsetX();

        double distance = std::fabs(pin_loc_min - pin_loc_max);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + width_epsilon_) * weight_adjust;
        weight = inv_p / (distance + width_epsilon_);
        if (!is_movable_min && is_movable_max) {
            bx[blk_num_max] += (pin_loc_min - offset_max) * weight;
            coefficientsx.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable_min && !is_movable_max) {
            bx[blk_num_min] += (pin_loc_max - offset_min) * weight;
            coefficientsx.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable_min && is_movable_max) {
            coefficientsx.emplace_back(blk_num_min, blk_num_min, weight);
            coefficientsx.emplace_back(blk_num_max, blk_num_max, weight);
            coefficientsx.emplace_back(blk_num_min, blk_num_max, -weight);
            coefficientsx.emplace_back(blk_num_max, blk_num_min, -weight);
            double offset_diff = (offset_max - offset_min) * weight;
            bx[blk_num_min] += offset_diff;
            bx[blk_num_max] -= offset_diff;
        }
    }

    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) {
            coefficientsx.emplace_back(i, i, 1);
            bx[i] = block_list[i].LLX();
        } else {
            if (block_list[i].LLX() < RegionLeft()
                || block_list[i].URX() > RegionRight()) {
                coefficientsx.emplace_back(i, i, center_weight);
                bx[i] += weight_center_x;
            }
        }
    }

    if (coefficients_capacity != coefficientsx.capacity()) {
        BOOST_LOG_TRIVIAL(warning)
            << "WARNING: coefficients capacity changed!\n"
            << "\told capacity: " << coefficients_capacity << "\n"
            << "\tnew capacity: " << coefficientsx.size() << "\n";
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_x += wall_time;
}

void GPSimPL::BuildProblemHPWLY() {
    double wall_time = get_wall_time();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t coefficients_capacity = coefficientsy.capacity();
    coefficientsy.resize(0);

    int sz = static_cast<int>(by.size());
    for (int i = 0; i < sz; ++i) {
        by[i] = 0;
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

    double weight;
    double weight_adjust;
    double inv_p;
    double decay_length = decay_factor * circuit_->AveBlkHeight();

    int max_pin_index;
    int blk_num_max;
    double pin_loc_max;
    bool is_movable_max;
    double offset_max;

    int min_pin_index;
    int blk_num_min;
    double pin_loc_min;
    bool is_movable_min;
    double offset_min;

    for (auto &net: *NetList()) {
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
        inv_p = net.InvP();
        net.UpdateMaxMinIdY();
        max_pin_index = net.MaxBlkPinIdY();
        min_pin_index = net.MinBlkPinIdY();

        blk_num_max = net.BlockPins()[max_pin_index].BlkId();
        pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
        is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
        offset_max = net.BlockPins()[max_pin_index].OffsetY();

        blk_num_min = net.BlockPins()[min_pin_index].BlkId();
        pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
        is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
        offset_min = net.BlockPins()[min_pin_index].OffsetY();

        double distance = std::fabs(pin_loc_min - pin_loc_max);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + height_epsilon_) * weight_adjust;
        weight = inv_p / (distance + height_epsilon_);
        if (!is_movable_min && is_movable_max) {
            by[blk_num_max] += (pin_loc_min - offset_max) * weight;
            coefficientsy.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable_min && !is_movable_max) {
            by[blk_num_min] += (pin_loc_max - offset_max) * weight;
            coefficientsy.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable_min && is_movable_max) {
            coefficientsy.emplace_back(blk_num_min, blk_num_min, weight);
            coefficientsy.emplace_back(blk_num_max, blk_num_max, weight);
            coefficientsy.emplace_back(blk_num_min, blk_num_max, -weight);
            coefficientsy.emplace_back(blk_num_max, blk_num_min, -weight);
            double offset_diff = (offset_max - offset_min) * weight;
            by[blk_num_min] += offset_diff;
            by[blk_num_max] -= offset_diff;
        }
    }
    for (int i = 0; i < sz;
         ++i) { // add the diagonal non-zero element for fixed blocks
        if (block_list[i].IsFixed()) {
            coefficientsy.emplace_back(i, i, 1);
            by[i] = block_list[i].LLY();
        } else {
            if (block_list[i].LLY() < RegionBottom()
                || block_list[i].URY() > RegionTop()) {
                coefficientsy.emplace_back(i, i, center_weight);
                by[i] += weight_center_y;
            }
        }
    }

    if (coefficients_capacity != coefficientsy.capacity()) {
        BOOST_LOG_TRIVIAL(warning)
            << "WARNING: coefficients capacity changed!\n"
            << "\told capacity: " << coefficients_capacity << "\n"
            << "\tnew capacity: " << coefficientsy.size() << "\n";
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_y += wall_time;
}

void GPSimPL::BuildProblemStarHPWLX() {
    double wall_time = get_wall_time();
    UpdateMaxMinX();

    int sz = bx.size();
    for (int i = 0; i < sz; ++i) {
        bx[i] = 0;
    }

    double decay_length = decay_factor * circuit_->AveBlkHeight();
    std::vector<BlkPairNets> &blk_pair_net_list = circuit_->blk_pair_net_list_;
    int pair_sz = blk_pair_net_list.size();
#pragma omp parallel for
    for (int i = 0; i < pair_sz; ++i) {
        BlkPairNets &blk_pair = blk_pair_net_list[i];
        blk_pair.ClearX();
        for (auto &edge: blk_pair.edges) {
            Net &net = *(edge.net);
            int d = edge.d;
            int l = edge.l;
            int driver_blk_num = net.BlockPins()[d].BlkId();
            double driver_pin_loc = net.BlockPins()[d].AbsX();
            bool driver_is_movable = net.BlockPins()[d].BlkPtr()->IsMovable();
            double driver_offset = net.BlockPins()[d].OffsetX();

            int load_blk_num = net.BlockPins()[l].BlkId();
            double load_pin_loc = net.BlockPins()[l].AbsX();
            bool load_is_movable = net.BlockPins()[l].BlkPtr()->IsMovable();
            double load_offset = net.BlockPins()[l].OffsetX();

            int max_pin_index = net.MaxBlkPinIdX();
            int min_pin_index = net.MinBlkPinIdX();

            int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
            double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
            bool is_movable_max =
                net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
            double offset_max = net.BlockPins()[max_pin_index].OffsetX();

            int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
            double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
            bool is_movable_min =
                net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
            double offset_min = net.BlockPins()[min_pin_index].OffsetX();

            double distance = std::fabs(load_pin_loc - driver_pin_loc);
            double weight_adjust = base_factor
                + adjust_factor * (1 - exp(-distance / decay_length));
            double inv_p = net.InvP();
            double weight = inv_p / (distance + width_epsilon_);
            weight *= weight_adjust;
            //weight = inv_p / (distance + width_epsilon_);
            double adjust = 1.0;
            if (driver_blk_num == blk_num_max) {
                adjust = (driver_pin_loc - load_pin_loc)
                    / (driver_pin_loc - pin_loc_min + width_epsilon_);
            } else if (driver_blk_num == blk_num_min) {
                adjust = (load_pin_loc - driver_pin_loc)
                    / (pin_loc_max - driver_pin_loc + width_epsilon_);
            } else {
                if (load_pin_loc > driver_pin_loc) {
                    adjust = (load_pin_loc - driver_pin_loc)
                        / (pin_loc_max - driver_pin_loc + width_epsilon_);
                } else {
                    adjust = (driver_pin_loc - load_pin_loc)
                        / (driver_pin_loc - pin_loc_min + width_epsilon_);
                }
            }
            //int exponent = cur_iter_/5;
            //weight *= std::pow(adjust, exponent);
            weight *= adjust;
            if (!load_is_movable && driver_is_movable) {
                if (driver_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0x += (load_pin_loc - driver_offset) * weight;
                    blk_pair.e00x += weight;
                } else {
                    blk_pair.b1x += (load_pin_loc - driver_offset) * weight;
                    blk_pair.e11x += weight;
                }
            } else if (load_is_movable && !driver_is_movable) {
                if (load_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0x += (driver_pin_loc - driver_offset) * weight;
                    blk_pair.e00x += weight;
                } else {
                    blk_pair.b1x += (driver_pin_loc - driver_offset) * weight;
                    blk_pair.e11x += weight;
                }
            } else if (load_is_movable && driver_is_movable) {
                double offset_diff = (driver_offset - load_offset) * weight;
                blk_pair.e00x += weight;
                blk_pair.e01x -= weight;
                blk_pair.e10x -= weight;
                blk_pair.e11x += weight;
                if (driver_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0x -= offset_diff;
                    blk_pair.b1x += offset_diff;
                } else {
                    blk_pair.b0x += offset_diff;
                    blk_pair.b1x -= offset_diff;
                }
            }
        }
        blk_pair.WriteX();
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
    std::vector<Block> &block_list = circuit_->Blocks();
#pragma omp parallel for
    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) {
            SpMat_diag_x[i].valueRef() = 1;
            bx[i] = block_list[i].LLX();
        } else {
            double diag_val = 0;
            double b = 0;
            for (auto &blk_pair: pair_connect[i]) {
                if (i == blk_pair->blk_num0) {
                    diag_val += blk_pair->e00x;
                    b += blk_pair->b0x;
                } else {
                    diag_val += blk_pair->e11x;
                    b += blk_pair->b1x;
                }
            }
            SpMat_diag_x[i].valueRef() = diag_val;
            bx[i] = b;
            if (block_list[i].LLX() < RegionLeft()
                || block_list[i].URX() > RegionRight()) {
                SpMat_diag_x[i].valueRef() += center_weight;
                bx[i] += weight_center_x;
            }
        }
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_x += wall_time;
}

void GPSimPL::BuildProblemStarHPWLY() {
    double wall_time = get_wall_time();
    UpdateMaxMinY();

    int sz = by.size();
    for (int i = 0; i < sz; ++i) {
        by[i] = 0;
    }

    double decay_length = decay_factor * circuit_->AveBlkHeight();
    std::vector<BlkPairNets> &blk_pair_net_list = circuit_->blk_pair_net_list_;
    int pair_sz = blk_pair_net_list.size();
#pragma omp parallel for
    for (int i = 0; i < pair_sz; ++i) {
        BlkPairNets &blk_pair = blk_pair_net_list[i];
        blk_pair.ClearY();
        for (auto &edge: blk_pair.edges) {
            Net &net = *(edge.net);
            int d = edge.d;
            int l = edge.l;
            int driver_blk_num = net.BlockPins()[d].BlkId();
            double driver_pin_loc = net.BlockPins()[d].AbsY();
            bool driver_is_movable = net.BlockPins()[d].BlkPtr()->IsMovable();
            double driver_offset = net.BlockPins()[d].OffsetY();

            int load_blk_num = net.BlockPins()[l].BlkId();
            double load_pin_loc = net.BlockPins()[l].AbsY();
            bool load_is_movable = net.BlockPins()[l].BlkPtr()->IsMovable();
            double load_offset = net.BlockPins()[l].OffsetY();

            int max_pin_index = net.MaxBlkPinIdY();
            int min_pin_index = net.MinBlkPinIdY();

            int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
            double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
            bool is_movable_max =
                net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
            double offset_max = net.BlockPins()[max_pin_index].OffsetY();

            int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
            double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
            bool is_movable_min =
                net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
            double offset_min = net.BlockPins()[min_pin_index].OffsetY();

            double distance = std::fabs(load_pin_loc - driver_pin_loc);
            double weight_adjust = base_factor
                + adjust_factor * (1 - exp(-distance / decay_length));
            double inv_p = net.InvP();
            double weight = inv_p / (distance + height_epsilon_);
            weight *= weight_adjust;
            //weight = inv_p / (distance + width_epsilon_);
            double adjust = 1.0;
            if (driver_blk_num == blk_num_max) {
                adjust = (driver_pin_loc - load_pin_loc)
                    / (driver_pin_loc - pin_loc_min + height_epsilon_);
            } else if (driver_blk_num == blk_num_min) {
                adjust = (load_pin_loc - driver_pin_loc)
                    / (pin_loc_max - driver_pin_loc + height_epsilon_);
            } else {
                if (load_pin_loc > driver_pin_loc) {
                    adjust = (load_pin_loc - driver_pin_loc)
                        / (pin_loc_max - driver_pin_loc + height_epsilon_);
                } else {
                    adjust = (driver_pin_loc - load_pin_loc)
                        / (driver_pin_loc - pin_loc_min + height_epsilon_);
                }
            }
            //int exponent = cur_iter_/5;
            //weight *= std::pow(adjust, exponent);
            weight *= adjust;
            if (!load_is_movable && driver_is_movable) {
                if (driver_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0y += (load_pin_loc - driver_offset) * weight;
                    blk_pair.e00y += weight;
                } else {
                    blk_pair.b1y += (load_pin_loc - driver_offset) * weight;
                    blk_pair.e11y += weight;
                }
            } else if (load_is_movable && !driver_is_movable) {
                if (load_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0y += (driver_pin_loc - driver_offset) * weight;
                    blk_pair.e00y += weight;
                } else {
                    blk_pair.b1y += (driver_pin_loc - driver_offset) * weight;
                    blk_pair.e11y += weight;
                }
            } else if (load_is_movable && driver_is_movable) {
                double offset_diff = (driver_offset - load_offset) * weight;
                blk_pair.e00y += weight;
                blk_pair.e01y -= weight;
                blk_pair.e10y -= weight;
                blk_pair.e11y += weight;
                if (driver_blk_num == blk_pair.blk_num0) {
                    blk_pair.b0y -= offset_diff;
                    blk_pair.b1y += offset_diff;
                } else {
                    blk_pair.b0y += offset_diff;
                    blk_pair.b1y -= offset_diff;
                }
            }
        }
        blk_pair.WriteY();
    }

    double center_weight = 0.03 / std::sqrt(sz);
    double
        weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;
    std::vector<Block> &block_list = circuit_->Blocks();
#pragma omp parallel for
    for (int i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) {
            SpMat_diag_y[i].valueRef() = 1;
            by[i] = block_list[i].LLY();
        } else {
            double diag_val = 0;
            double b = 0;
            for (auto &blk_pair: pair_connect[i]) {
                if (i == blk_pair->blk_num0) {
                    diag_val += blk_pair->e00y;
                    b += blk_pair->b0y;
                } else {
                    diag_val += blk_pair->e11y;
                    b += blk_pair->b1y;
                }
            }
            SpMat_diag_y[i].valueRef() = diag_val;
            by[i] = b;
            if (block_list[i].LLY() < RegionBottom()
                || block_list[i].URY() > RegionTop()) {
                SpMat_diag_y[i].valueRef() += center_weight;
                by[i] += weight_center_y;
            }
        }
    }

    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_y += wall_time;
}

double GPSimPL::OptimizeQuadraticMetricX(double cg_stop_criterion) {
    double wall_time = get_wall_time();
    if (net_model != 3) {
        Ax.setFromTriplets(coefficientsx.begin(), coefficientsx.end());
    }
    wall_time = get_wall_time() - wall_time;
    tot_matrix_from_triplets_x += wall_time;

    int sz = vx.size();
    std::vector<Block> &block_list = circuit_->Blocks();

    wall_time = get_wall_time();
    std::vector<double> eval_history;
    int max_rounds = cg_iteration_max_num_ / cg_iteration_;
    cgx.compute(Ax); // Ax * vx = bx
    for (int i = 0; i < max_rounds; ++i) {
        vx = cgx.solveWithGuess(bx, vx);
        for (int num = 0; num < sz; ++num) {
            block_list[num].SetLLX(vx[num]);
        }
        double evaluate_result = WeightedHPWLX();
        eval_history.push_back(evaluate_result);
        //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLX: %e\n", i, evaluate_result);
        if (eval_history.size() >= 3) {
            bool is_converge =
                IsSeriesConverge(eval_history, 3, cg_stop_criterion);
            bool is_oscillate = IsSeriesOscillate(eval_history, 5);
            if (is_converge) {
                break;
            }
            if (is_oscillate) {
                BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
                break;
            }
        }
    }
    BOOST_LOG_TRIVIAL(trace) << "      Metric optimization in X, sequence: "
                             << eval_history << "\n";
    wall_time = get_wall_time() - wall_time;
    tot_cg_solver_time_x += wall_time;

    wall_time = get_wall_time();

    for (int num = 0; num < sz; ++num) {
        block_list[num].SetLLX(vx[num]);
    }
    wall_time = get_wall_time() - wall_time;
    tot_loc_update_time_x += wall_time;

    DaliExpects(!eval_history.empty(),
                "Cannot return a valid value because the result is not evaluated!");
    return eval_history.back();
}

double GPSimPL::OptimizeQuadraticMetricY(double cg_stop_criterion) {
    double wall_time = get_wall_time();
    if (net_model != 3) {
        Ay.setFromTriplets(coefficientsy.begin(), coefficientsy.end());
    }
    wall_time = get_wall_time() - wall_time;
    tot_matrix_from_triplets_y += wall_time;

    int sz = vx.size();
    std::vector<Block> &block_list = circuit_->Blocks();

    wall_time = get_wall_time();
    std::vector<double> eval_history;
    int max_rounds = cg_iteration_max_num_ / cg_iteration_;
    cgy.compute(Ay);
    for (int i = 0; i < max_rounds; ++i) {
        vy = cgy.solveWithGuess(by, vy);
        for (int num = 0; num < sz; ++num) {
            block_list[num].SetLLY(vy[num]);
        }
        double evaluate_result = WeightedHPWLY();
        eval_history.push_back(evaluate_result);
        //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLY: %e\n", i, evaluate_result);
        if (eval_history.size() >= 3) {
            bool is_converge =
                IsSeriesConverge(eval_history, 3, cg_stop_criterion);
            bool is_oscillate = IsSeriesOscillate(eval_history, 5);
            if (is_converge) {
                break;
            }
            if (is_oscillate) {
                BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
                break;
            }
        }
    }
    BOOST_LOG_TRIVIAL(trace) << "      Metric optimization in Y, sequence: "
                             << eval_history << "\n";
    wall_time = get_wall_time() - wall_time;
    tot_cg_solver_time_y += wall_time;

    wall_time = get_wall_time();
    for (int num = 0; num < sz; ++num) {
        block_list[num].SetLLY(vy[num]);
    }
    wall_time = get_wall_time() - wall_time;
    tot_loc_update_time_y += wall_time;

    DaliExpects(!eval_history.empty(),
                "Cannot return a valid value because the result is not evaluated!");
    return eval_history.back();
}

void GPSimPL::PullBlockBackToRegion() {
    int sz = vx.size();
    std::vector<Block> &block_list = circuit_->Blocks();

    double blk_hi_bound_x;
    double blk_hi_bound_y;

    for (int num = 0; num < sz; ++num) {
        if (block_list[num].IsMovable()) {
            if (vx[num] < RegionLeft()) {
                vx[num] = RegionLeft();
            }
            blk_hi_bound_x = RegionRight() - block_list[num].Width();
            if (vx[num] > blk_hi_bound_x) {
                vx[num] = blk_hi_bound_x;
            }

            if (vy[num] < RegionBottom()) {
                vy[num] = RegionBottom();
            }
            blk_hi_bound_y = RegionTop() - block_list[num].Height();
            if (vy[num] > blk_hi_bound_y) {
                vy[num] = blk_hi_bound_y;
            }
        }
    }

    for (int num = 0; num < sz; ++num) {
        block_list[num].SetLoc(vx[num], vy[num]);
    }
}

void GPSimPL::BuildProblemX() {
    UpdateMaxMinX();
    if (net_model == 0) {
        BuildProblemB2BX();
    } else if (net_model == 1) {
        BuildProblemStarModelX();
    } else if (net_model == 2) {
        BuildProblemHPWLX();
    } else {
        BuildProblemStarHPWLX();
    }
}

void GPSimPL::BuildProblemY() {
    UpdateMaxMinY();
    if (net_model == 0) {
        BuildProblemB2BY();
    } else if (net_model == 1) {
        BuildProblemStarModelY();
    } else if (net_model == 2) {
        BuildProblemHPWLY();
    } else {
        BuildProblemStarHPWLY();
    }
}

double GPSimPL::QuadraticPlacement(double net_model_update_stop_criterion) {
    //omp_set_nested(1);
    omp_set_dynamic(0);
    int avail_threads_num = omp_get_max_threads();
    double wall_time = get_wall_time();

#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
    {
        //BOOST_LOG_TRIVIAL(info)  <<"OpenMP threads, %d\n", omp_get_num_threads());
        if (omp_get_thread_num() == 0) {
            omp_set_num_threads(avail_threads_num / 2);
            BOOST_LOG_TRIVIAL(trace) << "threads in branch x: "
                                     << omp_get_max_threads()
                                     << " Eigen threads: " << Eigen::nbThreads()
                                     << "\n";

            std::vector<Block> &block_list = circuit_->Blocks();
            for (size_t i = 0; i < block_list.size(); ++i) {
                vx[i] = block_list[i].LLX();
            }

            std::vector<double> eval_history_x;
            int b2b_update_it_x = 0;
            for (b2b_update_it_x = 0;
                 b2b_update_it_x < b2b_update_max_iteration_;
                 ++b2b_update_it_x) {
                BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
                BuildProblemX();
                double evaluate_result =
                    OptimizeQuadraticMetricX(cg_stop_criterion_);
                eval_history_x.push_back(evaluate_result);
                if (eval_history_x.size() >= 3) {
                    bool is_converge = IsSeriesConverge(eval_history_x,
                                                        3,
                                                        net_model_update_stop_criterion);
                    bool is_oscillate = IsSeriesOscillate(eval_history_x, 5);
                    if (is_converge) {
                        break;
                    }
                    if (is_oscillate) {
                        BOOST_LOG_TRIVIAL(trace)
                            << "Net model update oscillation detected X\n";
                        break;
                    }
                }
            }
            BOOST_LOG_TRIVIAL(trace)
                << "  Optimization summary X, iterations x: " << b2b_update_it_x
                << ", " << eval_history_x << "\n";
            DaliExpects(!eval_history_x.empty(),
                        "Cannot return a valid value because the result is not evaluated in GPSimPL::QuadraticPlacement()!");
            lower_bound_hpwlx_.push_back(eval_history_x.back());
        }

        if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
            omp_set_num_threads(avail_threads_num / 2);
            BOOST_LOG_TRIVIAL(trace) << "threads in branch y: "
                                     << omp_get_max_threads()
                                     << " Eigen threads: " << Eigen::nbThreads()
                                     << "\n";

            std::vector<Block> &block_list = circuit_->Blocks();
            for (size_t i = 0; i < block_list.size(); ++i) {
                vy[i] = block_list[i].LLY();
            }
            std::vector<double> eval_history_y;
            int b2b_update_it_y = 0;
            for (b2b_update_it_y = 0;
                 b2b_update_it_y < b2b_update_max_iteration_;
                 ++b2b_update_it_y) {
                BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
                BuildProblemY();
                double evaluate_result =
                    OptimizeQuadraticMetricY(cg_stop_criterion_);
                eval_history_y.push_back(evaluate_result);
                if (eval_history_y.size() >= 3) {
                    bool is_converge = IsSeriesConverge(eval_history_y,
                                                        3,
                                                        net_model_update_stop_criterion);
                    bool is_oscillate = IsSeriesOscillate(eval_history_y, 5);
                    if (is_converge) {
                        break;
                    }
                    if (is_oscillate) {
                        BOOST_LOG_TRIVIAL(trace)
                            << "Net model update oscillation detected Y\n";
                        break;
                    }
                }
            }
            BOOST_LOG_TRIVIAL(trace)
                << "  Optimization summary Y, iterations y: " << b2b_update_it_y
                << ", " << eval_history_y << "\n";
            DaliExpects(!eval_history_y.empty(),
                        "Cannot return a valid value because the result is not evaluated in GPSimPL::QuadraticPlacement()!");
            lower_bound_hpwly_.push_back(eval_history_y.back());
        }
    }

    PullBlockBackToRegion();

    BOOST_LOG_TRIVIAL(info) << "Initial Placement Complete\n";

    wall_time = get_wall_time() - wall_time;
    tot_cg_time += wall_time;
    //omp_set_nested(0);

    if (is_dump) DumpResult("cg_result_0.txt");

    return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
}

/****
* This function initialize the grid bin matrix, each bin has an area which can accommodate around number_of_cell_in_bin_ # of cells
* Part1
* grid_bin_height and grid_bin_width is determined by the following formula:
*    grid_bin_height = sqrt(number_of_cell_in_bin_ * average_area / filling_rate)
* the number of bins in the y-direction is given by:
*    grid_cnt_y = (Top() - Bottom())/grid_bin_height
*    grid_cnt_x = (Right() - Left())/grid_bin_width
* And initialize the space of grid_bin_matrix
*
* Part2
* for each grid bin, we need to initialize the attributes,
* including index, boundaries, area, and potential available white space
* the adjacent bin list is cached for the convenience of overfilled bin clustering
*
* Part3
* check whether the grid bin is occupied by fixed blocks, and we only do this once, and cache this result for future usage
* Assumption: fixed blocks do not overlap with each other!!!!!!!
* we will check whether this grid bin is covered by a fixed block,
* if yes, we set the label of this grid bin "all_terminal" to be true, initially, this value is false
* if not, we deduce the white space by the amount of fixed block and grid bin overlap region
* because a grid bin might be covered by more than one fixed blocks
* if the final white space is 0, we know the grid bin is also all covered by fixed blocks
* and we set the flag "all_terminal" to true.
* ****/
void GPSimPL::InitGridBins() {
    // Part1
    grid_bin_height = int(std::round(std::sqrt(
        number_of_cell_in_bin_ * GetCircuit()->AveMovBlkArea()
            / FillingRate())));
    grid_bin_width = grid_bin_height;
    grid_cnt_y =
        std::ceil(double(RegionTop() - RegionBottom()) / grid_bin_height);
    grid_cnt_x =
        std::ceil(double(RegionRight() - RegionLeft()) / grid_bin_width);
    BOOST_LOG_TRIVIAL(info) << "Global placement bin width, height: "
                            << grid_bin_width << "  "
                            << grid_bin_height << "\n";

    std::vector<GridBin> temp_grid_bin_column(grid_cnt_y);
    grid_bin_matrix.resize(grid_cnt_x, temp_grid_bin_column);

    // Part2
    for (int i = 0; i < grid_cnt_x; i++) {
        for (int j = 0; j < grid_cnt_y; j++) {
            grid_bin_matrix[i][j].index = {i, j};
            grid_bin_matrix[i][j].bottom = RegionBottom() + j * grid_bin_height;
            grid_bin_matrix[i][j].top =
                RegionBottom() + (j + 1) * grid_bin_height;
            grid_bin_matrix[i][j].left = RegionLeft() + i * grid_bin_width;
            grid_bin_matrix[i][j].right =
                RegionLeft() + (i + 1) * grid_bin_width;
            grid_bin_matrix[i][j].white_space = grid_bin_matrix[i][j].Area();
            // at the very beginning, assuming the white space is the same as area
            grid_bin_matrix[i][j].create_adjacent_bin_list(grid_cnt_x,
                                                           grid_cnt_y);
        }
    }

    // make sure the top placement boundary is the same as the top of the topmost bins
    for (int i = 0; i < grid_cnt_x; ++i) {
        grid_bin_matrix[i][grid_cnt_y - 1].top = RegionTop();
        grid_bin_matrix[i][grid_cnt_y - 1].white_space =
            grid_bin_matrix[i][grid_cnt_y - 1].Area();
    }
    // make sure the right placement boundary is the same as the right of the rightmost bins
    for (int i = 0; i < grid_cnt_y; ++i) {
        grid_bin_matrix[grid_cnt_x - 1][i].right = RegionRight();
        grid_bin_matrix[grid_cnt_x - 1][i].white_space =
            grid_bin_matrix[grid_cnt_x - 1][i].Area();
    }

    // Part3
    auto &block_list = Blocks();
    int sz = int(Blocks().size());
    int min_urx, max_llx, min_ury, max_lly;
    bool all_terminal, fixed_blk_out_of_region, blk_out_of_bin;
    int left_index, right_index, bottom_index, top_index;
    for (int i = 0; i < sz; i++) {
        /* find the left, right, bottom, top index of the grid */
        if (block_list[i].IsMovable()) continue;
        fixed_blk_out_of_region = int(block_list[i].LLX()) >= RegionRight() ||
            int(block_list[i].URX()) <= RegionLeft() ||
            int(block_list[i].LLY()) >= RegionTop() ||
            int(block_list[i].URY()) <= RegionBottom();
        if (fixed_blk_out_of_region) continue;
        left_index = (int) std::floor(
            (block_list[i].LLX() - RegionLeft()) / grid_bin_width);
        right_index = (int) std::floor(
            (block_list[i].URX() - RegionLeft()) / grid_bin_width);
        bottom_index = (int) std::floor(
            (block_list[i].LLY() - RegionBottom()) / grid_bin_height);
        top_index = (int) std::floor(
            (block_list[i].URY() - RegionBottom()) / grid_bin_height);
        /* the grid boundaries might be the placement region boundaries
     * if a block touches the rightmost and topmost boundaries, the index need to be fixed
     * to make sure no memory access out of scope */
        if (left_index < 0) left_index = 0;
        if (right_index >= grid_cnt_x) right_index = grid_cnt_x - 1;
        if (bottom_index < 0) bottom_index = 0;
        if (top_index >= grid_cnt_y) top_index = grid_cnt_y - 1;

        /* for each terminal, we will check which grid is inside it, and directly set the all_terminal attribute to true for that grid
     * some small terminals might occupy the same grid, we need to deduct the overlap area from the white space of that grid bin
     * when the final white space is 0, we know this grid bin is occupied by several terminals*/
        for (int j = left_index; j <= right_index; ++j) {
            for (int k = bottom_index; k <= top_index; ++k) {
                /* the following case might happen:
         * the top/right of a fixed block overlap with the bottom/left of a grid box
         * if this case happens, we need to ignore this fixed block for this grid box. */
                blk_out_of_bin =
                    int(block_list[i].LLX() >= grid_bin_matrix[j][k].right) ||
                        int(block_list[i].URX() <= grid_bin_matrix[j][k].left)
                        ||
                            int(block_list[i].LLY()
                                    >= grid_bin_matrix[j][k].top) ||
                        int(block_list[i].URY()
                                <= grid_bin_matrix[j][k].bottom);
                if (blk_out_of_bin) continue;
                grid_bin_matrix[j][k].terminal_list.push_back(i);

                // if grid bin is covered by a large fixed block, then all_terminal flag for this block is set to be true
                all_terminal =
                    int(block_list[i].LLX() <= grid_bin_matrix[j][k].LLX()) &&
                        int(block_list[i].LLY() <= grid_bin_matrix[j][k].LLY())
                        &&
                            int(block_list[i].URX()
                                    >= grid_bin_matrix[j][k].URX()) &&
                        int(block_list[i].URY() >= grid_bin_matrix[j][k].URY());
                grid_bin_matrix[j][k].all_terminal = all_terminal;
                // if all_terminal flag is false, we need to calculate the overlap of grid bin and this fixed block to get the white space,
                // when white space is less than 1, this grid bin is also all_terminal
                if (all_terminal) {
                    grid_bin_matrix[j][k].white_space = 0;
                } else {
                    // this part calculate the overlap of two rectangles
                    max_llx = std::max(int(block_list[i].LLX()),
                                       grid_bin_matrix[j][k].LLX());
                    max_lly = std::max(int(block_list[i].LLY()),
                                       grid_bin_matrix[j][k].LLY());
                    min_urx = std::min(int(block_list[i].URX()),
                                       grid_bin_matrix[j][k].URX());
                    min_ury = std::min(int(block_list[i].URY()),
                                       grid_bin_matrix[j][k].URY());
                    grid_bin_matrix[j][k].white_space -=
                        (unsigned long int) (min_urx - max_llx)
                            * (min_ury - max_lly);
                    if (grid_bin_matrix[j][k].white_space < 1) {
                        grid_bin_matrix[j][k].all_terminal = true;
                        grid_bin_matrix[j][k].white_space = 0;
                    }
                }
            }
        }
    }
}

void GPSimPL::InitWhiteSpaceLUT() {
    /****
   * this is a member function to initialize white space look-up table
   * this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in this region
   * an easier way is to define an accumulate function and store it as a look-up table
   * when we want to find the white space in a region, the value can be easily extracted from the look-up table
   * ****/

    // this for loop is created to initialize the size of the loop-up table
    std::vector<unsigned long int> tmp_vector(grid_cnt_y);
    grid_bin_white_space_LUT.resize(grid_cnt_x, tmp_vector);

    // this for loop is used for computing elements in the look-up table
    // there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
    for (int kx = 0; kx < grid_cnt_x; ++kx) {
        for (int ky = 0; ky < grid_cnt_y; ++ky) {
            grid_bin_white_space_LUT[kx][ky] = 0;
            if (kx == 0) {
                if (ky == 0) {
                    grid_bin_white_space_LUT[kx][ky] =
                        grid_bin_matrix[0][0].white_space;
                } else {
                    grid_bin_white_space_LUT[kx][ky] =
                        grid_bin_white_space_LUT[kx][ky - 1]
                            + grid_bin_matrix[kx][ky].white_space;
                }
            } else {
                if (ky == 0) {
                    grid_bin_white_space_LUT[kx][ky] =
                        grid_bin_white_space_LUT[kx - 1][ky]
                            + grid_bin_matrix[kx][ky].white_space;
                } else {
                    grid_bin_white_space_LUT[kx][ky] =
                        grid_bin_white_space_LUT[kx - 1][ky]
                            + grid_bin_white_space_LUT[kx][ky - 1]
                            + grid_bin_matrix[kx][ky].white_space
                            - grid_bin_white_space_LUT[kx - 1][ky - 1];
                }
            }
        }
    }
}

void GPSimPL::LALInit() {
    upper_bound_hpwlx_.clear();
    upper_bound_hpwly_.clear();
    upper_bound_hpwl_.clear();
    InitGridBins();
    InitWhiteSpaceLUT();
}

void GPSimPL::LALClose() {
    grid_bin_matrix.clear();
    grid_bin_white_space_LUT.clear();
}

void GPSimPL::ClearGridBinFlag() {
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) bin.global_placed = false;
    }
}

unsigned long int GPSimPL::LookUpWhiteSpace(GridBinIndex const &ll_index,
                                            GridBinIndex const &ur_index) {
    /****
   * this function is used to return the white space in a region specified by ll_index, and ur_index
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
   * ****/

    unsigned long int total_white_space;
    /*if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1]
          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y]
          + grid_bin_white_space_LUT[ll_index.x-1][ll_index.y-1];
    }
  }*/
    WindowQuadruple window = {ll_index.x, ll_index.y, ur_index.x, ur_index.y};
    total_white_space = LookUpWhiteSpace(window);
    return total_white_space;
}

unsigned long int GPSimPL::LookUpWhiteSpace(WindowQuadruple &window) {
    unsigned long int total_white_space;
    if (window.llx == 0) {
        if (window.lly == 0) {
            total_white_space =
                grid_bin_white_space_LUT[window.urx][window.ury];
        } else {
            total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
                - grid_bin_white_space_LUT[window.urx][window.lly - 1];
        }
    } else {
        if (window.lly == 0) {
            total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
                - grid_bin_white_space_LUT[window.llx - 1][window.ury];
        } else {
            total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
                - grid_bin_white_space_LUT[window.urx][window.lly - 1]
                - grid_bin_white_space_LUT[window.llx - 1][window.ury]
                + grid_bin_white_space_LUT[window.llx - 1][window.lly - 1];
        }
    }
    return total_white_space;
}

unsigned long int GPSimPL::LookUpBlkArea(WindowQuadruple &window) {
    unsigned long int res = 0;
    for (int x = window.llx; x <= window.urx; ++x) {
        for (int y = window.lly; y <= window.ury; ++y) {
            res += grid_bin_matrix[x][y].cell_area;
        }
    }
    return res;
}

unsigned long int GPSimPL::WindowArea(WindowQuadruple &window) {
    unsigned long int res = 0;
    if (window.urx == grid_cnt_x - 1) {
        if (window.ury == grid_cnt_y - 1) {
            res = (window.urx - window.llx) * (window.ury - window.lly)
                * grid_bin_width * grid_bin_height;
            res += (window.urx - window.llx) * grid_bin_width
                * grid_bin_matrix[window.urx][window.ury].Height();
            res += (window.ury - window.lly) * grid_bin_height
                * grid_bin_matrix[window.urx][window.ury].Width();
            res += grid_bin_matrix[window.urx][window.ury].Area();
        } else {
            res = (window.urx - window.llx) * (window.ury - window.lly + 1)
                * grid_bin_width * grid_bin_height;
            res += (window.ury - window.lly + 1) * grid_bin_height
                * grid_bin_matrix[window.urx][window.ury].Width();
        }
    } else {
        if (window.ury == grid_cnt_y - 1) {
            res = (window.urx - window.llx + 1) * (window.ury - window.lly)
                * grid_bin_width * grid_bin_height;
            res += (window.urx - window.llx + 1) * grid_bin_width
                * grid_bin_matrix[window.urx][window.ury].Height();
        } else {
            res = (window.urx - window.llx + 1) * (window.ury - window.lly + 1)
                * grid_bin_width * grid_bin_height;
        }
    }
    /*
  for (int i=window.lx; i<=window.urx; ++i) {
    for (int j=window.lly; j<=window.ury; ++j) {
      res += grid_bin_matrix[i][j].Area();
    }
  }*/
    return res;
}

void GPSimPL::UpdateGridBinState() {
    /****
   * this is a member function to update grid bin status, because the cell_list, cell_area and over_fill state can be changed
   * so we need to update them when necessary
   * ****/

    double wall_time = get_wall_time();

    // clean the old data
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            bin.cell_list.clear();
            bin.cell_area = 0;
            bin.over_fill = false;
        }
    }

    // for each cell, find the index of the grid bin it should be in
    // note that in extreme cases, the index might be smaller than 0 or larger than the maximum allowed index
    // because the cell is on the boundaries, so we need to make some modifications for these extreme cases
    std::vector<Block> &block_list = circuit_->Blocks();
    int sz = int(block_list.size());
    int x_index = 0;
    int y_index = 0;

    //double for_time = get_wall_time();
    for (int i = 0; i < sz; i++) {
        if (block_list[i].IsFixed()) continue;
        x_index = (int) std::floor(
            (block_list[i].X() - RegionLeft()) / grid_bin_width);
        y_index = (int) std::floor(
            (block_list[i].Y() - RegionBottom()) / grid_bin_height);
        if (x_index < 0) x_index = 0;
        if (x_index > grid_cnt_x - 1) x_index = grid_cnt_x - 1;
        if (y_index < 0) y_index = 0;
        if (y_index > grid_cnt_y - 1) y_index = grid_cnt_y - 1;
        grid_bin_matrix[x_index][y_index].cell_list.push_back(i);
        grid_bin_matrix[x_index][y_index].cell_area += block_list[i].Area();
    }
    //for_time = get_wall_time() - for_time;
    //BOOST_LOG_TRIVIAL(info)   << "for time: " << for_time << "s\n";
    //exit(1);

    /**** below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by fixed blocks, but its cell_list is non-empty, which means there is some cells overlap with this grid bin, we say it is over_fill
   * 2. if not fully occupied by terminals, but filling_rate is larger than the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with fixed blocks in this bin, we also mark it as over_fill
   * ****/
    //TODO: the third criterion might be changed in the next
    bool over_fill = false;
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            if (bin.global_placed) {
                bin.over_fill = false;
                continue;
            }
            if (bin.IsAllFixedBlk()) {
                if (!bin.cell_list.empty()) {
                    bin.over_fill = true;
                }
            } else {
                bin.filling_rate =
                    double(bin.cell_area) / double(bin.white_space);
                if (bin.filling_rate > FillingRate()) {
                    bin.over_fill = true;
                }
            }
            if (!bin.OverFill()) {
                for (auto &cell_num: bin.cell_list) {
                    for (auto &terminal_num: bin.terminal_list) {
                        over_fill =
                            block_list[cell_num].IsOverlap(block_list[terminal_num]);
                        if (over_fill) {
                            bin.over_fill = true;
                            break;
                        }
                    }
                    if (over_fill) break;
                    // two breaks have to be used to break two loops
                }
            }
        }
    }
    UpdateGridBinState_time += get_wall_time() - wall_time;
}

void GPSimPL::UpdateClusterList() {
    double wall_time = get_wall_time();
    cluster_set.clear();

    int m = (int) grid_bin_matrix.size(); // number of rows
    int n = (int) grid_bin_matrix[0].size(); // number of columns
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j)
            grid_bin_matrix[i][j].cluster_visited = false;
    }
    int cnt = 0;
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            if (grid_bin_matrix[i][j].cluster_visited
                || !grid_bin_matrix[i][j].over_fill)
                continue;
            GridBinIndex b(i, j);
            GridBinCluster H;
            H.bin_set.insert(b);
            grid_bin_matrix[i][j].cluster_visited = true;
            cnt = 0;
            std::queue<GridBinIndex> Q;
            Q.push(b);
            while (!Q.empty()) {
                b = Q.front();
                Q.pop();
                for (auto
                        &index: grid_bin_matrix[b.x][b.y].adjacent_bin_index) {
                    GridBin &bin = grid_bin_matrix[index.x][index.y];
                    if (!bin.cluster_visited && bin.over_fill) {
                        if (cnt > cluster_upper_size) {
                            UpdateClusterArea(H);
                            cluster_set.insert(H);
                            break;
                        }
                        bin.cluster_visited = true;
                        H.bin_set.insert(index);
                        ++cnt;
                        Q.push(index);
                    }
                }
            }
            UpdateClusterArea(H);
            cluster_set.insert(H);
        }
    }
    UpdateClusterList_time += get_wall_time() - wall_time;
}

void GPSimPL::UpdateLargestCluster() {
    if (cluster_set.empty()) return;

    for (auto it = cluster_set.begin(); it != cluster_set.end();) {
        bool is_contact = true;

        // if there is no grid bin has been roughly legalized, then this cluster is the largest one for sure
        for (auto &index: it->bin_set) {
            if (grid_bin_matrix[index.x][index.y].global_placed) {
                is_contact = false;
            }
        }
        if (is_contact) break;

        // initialize a list to store all indices in this cluster
        // initialize a map to store the visited flag during bfs
        std::vector<GridBinIndex> grid_bin_list;
        grid_bin_list.reserve(it->bin_set.size());
        std::unordered_map<GridBinIndex, bool, GridBinIndexHasher>
            grid_bin_visited;
        for (auto &index: it->bin_set) {
            grid_bin_list.push_back(index);
            grid_bin_visited.insert({index, false});
        }

        std::sort(grid_bin_list.begin(),
                  grid_bin_list.end(),
                  [](const GridBinIndex &index1, const GridBinIndex &index2) {
                      return (index1.x < index2.x)
                          || (index1.x == index2.x && index1.y < index2.y);
                  });

        int cnt = 0;
        for (auto &grid_index: grid_bin_list) {
            int i = grid_index.x;
            int j = grid_index.y;
            if (grid_bin_visited[grid_index]) continue; // if this grid bin has been visited continue
            if (grid_bin_matrix[i][j].global_placed) continue; // if this grid bin has been roughly legalized
            GridBinIndex b(i, j);
            GridBinCluster H;
            H.bin_set.insert(b);
            grid_bin_visited[grid_index] = true;
            cnt = 0;
            std::queue<GridBinIndex> Q;
            Q.push(b);
            while (!Q.empty()) {
                b = Q.front();
                Q.pop();
                for (auto
                        &index: grid_bin_matrix[b.x][b.y].adjacent_bin_index) {
                    if (grid_bin_visited.find(index)
                        == grid_bin_visited.end())
                        continue; // this index is not in the cluster
                    if (grid_bin_visited[index]) continue; // this index has been visited
                    if (grid_bin_matrix[index.x][index.y].global_placed) continue; // if this grid bin has been roughly legalized
                    GridBin &bin = grid_bin_matrix[index.x][index.y];
                    if (cnt > cluster_upper_size) {
                        UpdateClusterArea(H);
                        cluster_set.insert(H);
                        break;
                    }
                    grid_bin_visited[index] = true;
                    H.bin_set.insert(index);
                    ++cnt;
                    Q.push(index);
                }
            }
            UpdateClusterArea(H);
            cluster_set.insert(H);
        }

        it = cluster_set.erase(it);
    }
}

void GPSimPL::FindMinimumBoxForLargestCluster() {
    /****
   * this function find the box for the largest cluster,
   * such that the total white space in the box is larger than the total cell area
   * the way to do this is just by expanding the boundaries of the bounding box of the first cluster
   *
   * Part 1
   * find the index of the maximum cluster
   * ****/

    double wall_time = get_wall_time();

    // clear the queue_box_bin
    while (!queue_box_bin.empty()) queue_box_bin.pop();
    if (cluster_set.empty()) return;

    // Part 1
    std::vector<Block> &block_list = circuit_->Blocks();

    BoxBin R;
    R.cut_direction_x = false;

    R.ll_index.x = grid_cnt_x - 1;
    R.ll_index.y = grid_cnt_y - 1;
    R.ur_index.x = 0;
    R.ur_index.y = 0;
    // initialize a box with y cut-direction
    // identify the bounding box of the initial cluster
    auto it = cluster_set.begin();
    for (auto &index: it->bin_set) {
        R.ll_index.x = std::min(R.ll_index.x, index.x);
        R.ur_index.x = std::max(R.ur_index.x, index.x);
        R.ll_index.y = std::min(R.ll_index.y, index.y);
        R.ur_index.y = std::max(R.ur_index.y, index.y);
    }
    while (true) {
        // update cell area, white space, and thus filling rate to determine whether to expand this box or not
        R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
        R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                              grid_bin_matrix);
        if (R.filling_rate > FillingRate()) {
            R.ExpandBox(grid_cnt_x, grid_cnt_y);
        } else {
            break;
        }
        //BOOST_LOG_TRIVIAL(info)   << R.total_white_space << "  " << R.filling_rate << "  " << FillingRate() << "\n";
    }

    R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
    R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                          grid_bin_matrix);
    R.UpdateCellList(grid_bin_matrix);
    R.ll_point.x = grid_bin_matrix[R.ll_index.x][R.ll_index.y].left;
    R.ll_point.y = grid_bin_matrix[R.ll_index.x][R.ll_index.y].bottom;
    R.ur_point.x = grid_bin_matrix[R.ur_index.x][R.ur_index.y].right;
    R.ur_point.y = grid_bin_matrix[R.ur_index.x][R.ur_index.y].top;

    R.left = int(R.ll_point.x);
    R.bottom = int(R.ll_point.y);
    R.right = int(R.ur_point.x);
    R.top = int(R.ur_point.y);

    if (R.ll_index == R.ur_index) {
        R.UpdateFixedBlkList(grid_bin_matrix);
        if (R.IsContainFixedBlk()) {
            R.UpdateObsBoundary(block_list);
        }
    }
    queue_box_bin.push(R);
    //BOOST_LOG_TRIVIAL(info)   << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
    //BOOST_LOG_TRIVIAL(info)   << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

    for (int kx = R.ll_index.x; kx <= R.ur_index.x; ++kx) {
        for (int ky = R.ll_index.y; ky <= R.ur_index.y; ++ky) {
            grid_bin_matrix[kx][ky].global_placed = true;
        }
    }

    FindMinimumBoxForLargestCluster_time += get_wall_time() - wall_time;
}

void GPSimPL::SplitBox(BoxBin &box) {
    std::vector<Block> &block_list = circuit_->Blocks();
    bool flag_bisection_complete;
    int dominating_box_flag; // indicate whether there is a dominating BoxBin
    BoxBin box1, box2;
    box1.ll_index = box.ll_index;
    box2.ur_index = box.ur_index;
    // this part of code can be simplified, but after which the code might be unclear
    // cut-line along vertical direction
    if (box.cut_direction_x) {
        flag_bisection_complete =
            box.update_cut_index_white_space(grid_bin_white_space_LUT);
        if (flag_bisection_complete) {
            box1.cut_direction_x = false;
            box2.cut_direction_x = false;
            box1.ur_index = box.cut_ur_index;
            box2.ll_index = box.cut_ll_index;
        } else {
            // if bisection fail in one direction, do bisection in the other direction
            box.cut_direction_x = false;
            flag_bisection_complete =
                box.update_cut_index_white_space(grid_bin_white_space_LUT);
            if (flag_bisection_complete) {
                box1.cut_direction_x = false;
                box2.cut_direction_x = false;
                box1.ur_index = box.cut_ur_index;
                box2.ll_index = box.cut_ll_index;
            }
        }
    } else {
        // cut-line along horizontal direction
        flag_bisection_complete =
            box.update_cut_index_white_space(grid_bin_white_space_LUT);
        if (flag_bisection_complete) {
            box1.cut_direction_x = true;
            box2.cut_direction_x = true;
            box1.ur_index = box.cut_ur_index;
            box2.ll_index = box.cut_ll_index;
        } else {
            box.cut_direction_x = true;
            flag_bisection_complete =
                box.update_cut_index_white_space(grid_bin_white_space_LUT);
            if (flag_bisection_complete) {
                box1.cut_direction_x = true;
                box2.cut_direction_x = true;
                box1.ur_index = box.cut_ur_index;
                box2.ll_index = box.cut_ll_index;
            }
        }
    }
    box1.update_cell_area_white_space(grid_bin_matrix);
    box2.update_cell_area_white_space(grid_bin_matrix);
    //BOOST_LOG_TRIVIAL(info)   << box1.ll_index_ << box1.ur_index_ << "\n";
    //BOOST_LOG_TRIVIAL(info)   << box2.ll_index_ << box2.ur_index_ << "\n";
    //box1.update_all_terminal(grid_bin_matrix);
    //box2.update_all_terminal(grid_bin_matrix);
    // if the white space in one bin is dominating the other, ignore the smaller one
    dominating_box_flag = 0;
    if (double(box1.total_white_space) / double(box.total_white_space)
        <= 0.01) {
        dominating_box_flag = 1;
    }
    if (double(box2.total_white_space) / double(box.total_white_space)
        <= 0.01) {
        dominating_box_flag = 2;
    }

    box1.update_boundaries(grid_bin_matrix);
    box2.update_boundaries(grid_bin_matrix);

    if (dominating_box_flag == 0) {
        //BOOST_LOG_TRIVIAL(info)   << "cell list size: " << box.cell_list.size() << "\n";
        //box.update_cell_area(block_list);
        //BOOST_LOG_TRIVIAL(info)   << "total_cell_area: " << box.total_cell_area << "\n";
        box.update_cut_point_cell_list_low_high(block_list,
                                                box1.total_white_space,
                                                box2.total_white_space);
        box1.cell_list = box.cell_list_low;
        box2.cell_list = box.cell_list_high;
        box1.ll_point = box.ll_point;
        box2.ur_point = box.ur_point;
        box1.ur_point = box.cut_ur_point;
        box2.ll_point = box.cut_ll_point;
        box1.total_cell_area = box.total_cell_area_low;
        box2.total_cell_area = box.total_cell_area_high;

        if (box1.ll_index == box1.ur_index) {
            box1.UpdateFixedBlkList(grid_bin_matrix);
            box1.UpdateObsBoundary(block_list);
        }
        if (box2.ll_index == box2.ur_index) {
            box2.UpdateFixedBlkList(grid_bin_matrix);
            box2.UpdateObsBoundary(block_list);
        }

        /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      BOOST_LOG_TRIVIAL(info)   << box1.left << " " << box1.bottom << "\n";
    }
    if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      BOOST_LOG_TRIVIAL(info)   << box2.left << " " << box2.bottom << "\n";
    }*/

        queue_box_bin.push(box1);
        queue_box_bin.push(box2);
        //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
        //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
        //box1.write_cell_region("first_cell_bounding_box.txt");
        //box2.write_cell_region("first_cell_bounding_box.txt");
    } else if (dominating_box_flag == 1) {
        box2.ll_point = box.ll_point;
        box2.ur_point = box.ur_point;
        box2.cell_list = box.cell_list;
        box2.total_cell_area = box.total_cell_area;
        if (box2.ll_index == box2.ur_index) {
            box2.UpdateFixedBlkList(grid_bin_matrix);
            box2.UpdateObsBoundary(block_list);
        }

        /*if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      BOOST_LOG_TRIVIAL(info)   << box2.left << " " << box2.bottom << "\n";
    }*/

        queue_box_bin.push(box2);
        //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
        //box2.write_cell_region("first_cell_bounding_box.txt");
    } else {
        box1.ll_point = box.ll_point;
        box1.ur_point = box.ur_point;
        box1.cell_list = box.cell_list;
        box1.total_cell_area = box.total_cell_area;
        if (box1.ll_index == box1.ur_index) {
            box1.UpdateFixedBlkList(grid_bin_matrix);
            box1.UpdateObsBoundary(block_list);
        }

        /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      BOOST_LOG_TRIVIAL(info)   << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      BOOST_LOG_TRIVIAL(info)   << box1.left << " " << box1.bottom << "\n";
    }*/

        queue_box_bin.push(box1);
        //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
        //box1.write_cell_region("first_cell_bounding_box.txt");
    }
}

void GPSimPL::SplitGridBox(BoxBin &box) {
    std::vector<Block> &block_list = circuit_->Blocks();
    BoxBin box1, box2;
    box1.left = box.left;
    box1.bottom = box.bottom;
    box2.right = box.right;
    box2.top = box.top;
    box1.ll_index = box.ll_index;
    box1.ur_index = box.ur_index;
    box2.ll_index = box.ll_index;
    box2.ur_index = box.ur_index;
    /* split along the direction with more boundary lines */
    if (box.horizontal_obstacle_boundaries.size()
        > box.vertical_obstacle_boundaries.size()) {
        box.cut_direction_x = true;
        box1.right = box.right;
        box1.top = box.horizontal_obstacle_boundaries[0];
        box2.left = box.left;
        box2.bottom = box.horizontal_obstacle_boundaries[0];
        box1.update_terminal_list_white_space(block_list, box.terminal_list);
        box2.update_terminal_list_white_space(block_list, box.terminal_list);

        if (double(box1.total_white_space) / (double) box.total_white_space
            <= 0.01) {
            box2.ll_point = box.ll_point;
            box2.ur_point = box.ur_point;
            box2.cell_list = box.cell_list;
            box2.total_cell_area = box.total_cell_area;
            box2.UpdateObsBoundary(block_list);
            queue_box_bin.push(box2);
        } else if (
            double(box2.total_white_space) / (double) box.total_white_space
                <= 0.01) {
            box1.ll_point = box.ll_point;
            box1.ur_point = box.ur_point;
            box1.cell_list = box.cell_list;
            box1.total_cell_area = box.total_cell_area;
            box1.UpdateObsBoundary(block_list);
            queue_box_bin.push(box1);
        } else {
            box.update_cut_point_cell_list_low_high(block_list,
                                                    box1.total_white_space,
                                                    box2.total_white_space);
            box1.cell_list = box.cell_list_low;
            box2.cell_list = box.cell_list_high;
            box1.ll_point = box.ll_point;
            box2.ur_point = box.ur_point;
            box1.ur_point = box.cut_ur_point;
            box2.ll_point = box.cut_ll_point;
            box1.total_cell_area = box.total_cell_area_low;
            box2.total_cell_area = box.total_cell_area_high;
            box1.UpdateObsBoundary(block_list);
            box2.UpdateObsBoundary(block_list);
            queue_box_bin.push(box1);
            queue_box_bin.push(box2);
        }
    } else {
        box.cut_direction_x = false;
        box1.right = box.vertical_obstacle_boundaries[0];
        box1.top = box.top;
        box2.left = box.vertical_obstacle_boundaries[0];
        box2.bottom = box.bottom;
        box1.update_terminal_list_white_space(block_list, box.terminal_list);
        box2.update_terminal_list_white_space(block_list, box.terminal_list);

        if (double(box1.total_white_space) / (double) box.total_white_space
            <= 0.01) {
            box2.ll_point = box.ll_point;
            box2.ur_point = box.ur_point;
            box2.cell_list = box.cell_list;
            box2.total_cell_area = box.total_cell_area;
            box2.UpdateObsBoundary(block_list);
            queue_box_bin.push(box2);
        } else if (
            double(box2.total_white_space) / (double) box.total_white_space
                <= 0.01) {
            box1.ll_point = box.ll_point;
            box1.ur_point = box.ur_point;
            box1.cell_list = box.cell_list;
            box1.total_cell_area = box.total_cell_area;
            box1.UpdateObsBoundary(block_list);
            queue_box_bin.push(box1);
        } else {
            box.update_cut_point_cell_list_low_high(block_list,
                                                    box1.total_white_space,
                                                    box2.total_white_space);
            box1.cell_list = box.cell_list_low;
            box2.cell_list = box.cell_list_high;
            box1.ll_point = box.ll_point;
            box2.ur_point = box.ur_point;
            box1.ur_point = box.cut_ur_point;
            box2.ll_point = box.cut_ll_point;
            box1.total_cell_area = box.total_cell_area_low;
            box2.total_cell_area = box.total_cell_area_high;
            box1.UpdateObsBoundary(block_list);
            box2.UpdateObsBoundary(block_list);
            queue_box_bin.push(box1);
            queue_box_bin.push(box2);
        }
    }
}

void GPSimPL::PlaceBlkInBox(BoxBin &box) {
    std::vector<Block> &block_list = circuit_->Blocks();
    /* this is the simplest version, just linearly move cells in the cell_box to the grid box
  * non-linearity is not considered yet*/

    /*double cell_box_left, cell_box_bottom;
  double cell_box_width, cell_box_height;
  cell_box_left = box.ll_point.x;
  cell_box_bottom = box.ll_point.y;
  cell_box_width = box.ur_point.x - cell_box_left;
  cell_box_height = box.ur_point.y - cell_box_bottom;
  Block *cell;

  for (auto &cell_id: box.cell_list) {
    cell = &block_list[cell_id];
    cell->SetCenterX((cell->X() - cell_box_left)/cell_box_width * (box.right - box.left) + box.left);
    cell->SetCenterY((cell->Y() - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom);
  }*/

    int sz = box.cell_list.size();
    std::vector<std::pair<int, double>> index_loc_list_x(sz);
    std::vector<std::pair<int, double>> index_loc_list_y(sz);
    GridBin &grid_bin = grid_bin_matrix[box.ll_index.x][box.ll_index.y];
    for (int i = 0; i < sz; ++i) {
        int blk_num = box.cell_list[i];
        index_loc_list_x[i].first = blk_num;
        index_loc_list_x[i].second = block_list[blk_num].X();
        index_loc_list_y[i].first = blk_num;
        index_loc_list_y[i].second = block_list[blk_num].Y();
        grid_bin.cell_list.push_back(blk_num);
        grid_bin.cell_area += block_list[blk_num].Area();
    }

    std::sort(index_loc_list_x.begin(),
              index_loc_list_x.end(),
              [](const std::pair<int, double> &p1,
                 const std::pair<int, double> &p2) {
                  return p1.second < p2.second;
              });
    double total_length = 0;
    for (auto &cell_id: box.cell_list) {
        total_length += block_list[cell_id].Width();
    }
    double cur_pos = 0;
    int box_width = box.right - box.left;
    int cell_num;
    for (auto &pair: index_loc_list_x) {
        cell_num = pair.first;
        block_list[cell_num].SetCenterX(
            box.left + cur_pos / total_length * box_width);
        cur_pos += block_list[cell_num].Width();
    }

    std::sort(index_loc_list_y.begin(),
              index_loc_list_y.end(),
              [](const std::pair<int, double> &p1,
                 const std::pair<int, double> &p2) {
                  return p1.second < p2.second;
              });
    total_length = 0;
    for (auto &cell_id: box.cell_list) {
        total_length += block_list[cell_id].Height();
    }
    cur_pos = 0;
    int box_height = box.top - box.bottom;
    for (auto &pair: index_loc_list_y) {
        cell_num = pair.first;
        block_list[cell_num].SetCenterY(
            box.bottom + cur_pos / total_length * box_height);
        cur_pos += block_list[cell_num].Height();
    }
}

void GPSimPL::RoughLegalBlkInBox(BoxBin &box) {
    int sz = box.cell_list.size();
    if (sz == 0) return;
    std::vector<int> row_start;
    row_start.assign(box.top - box.bottom + 1, box.left);

    std::vector<IndexLocPair<int>> index_loc_list;
    IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
    index_loc_list.resize(sz, tmp_index_loc_pair);

    std::vector<Block> &block_list = circuit_->Blocks();
    int blk_num;
    for (int i = 0; i < sz; ++i) {
        blk_num = box.cell_list[i];
        index_loc_list[i].num = blk_num;
        index_loc_list[i].x = block_list[blk_num].LLX();
        index_loc_list[i].y = block_list[blk_num].LLY();
    }
    std::sort(index_loc_list.begin(), index_loc_list.end());

    int init_x;
    int init_y;
    int height;
    int width;
    int start_row;
    int end_row;
    int best_row;
    int best_loc;
    double min_cost;
    double tmp_cost;
    int tmp_end_row;
    int tmp_x;
    int tmp_y;

    double min_x = right_;
    double max_x = left_;

    for (auto &pair: index_loc_list) {
        auto &block = block_list[pair.num];

        init_x = int(block.LLX());
        init_y = int(block.LLY());

        height = int(block.Height());
        width = int(block.Width());
        start_row = std::max(0, init_y - box.bottom - 2 * height);
        end_row = std::min(box.top - box.bottom - height,
                           init_y - box.bottom + 3 * height);

        best_row = 0;
        best_loc = INT_MIN;
        min_cost = DBL_MAX;

        for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
            tmp_end_row = tmp_row + height - 1;
            tmp_x = std::max(box.left, init_x - 1 * width);
            for (int i = tmp_row; i <= tmp_end_row; ++i) {
                tmp_x = std::max(tmp_x, row_start[i]);
            }

            tmp_y = tmp_row + box.bottom;
            //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

            tmp_cost = std::abs(tmp_x - init_x) + std::abs(tmp_y - init_y);
            if (tmp_cost < min_cost) {
                best_loc = tmp_x;
                best_row = tmp_row;
                min_cost = tmp_cost;
            }
        }

        int res_x = best_loc;
        int res_y = best_row + box.bottom;

        //BOOST_LOG_TRIVIAL(info)   << res_x << "  " << res_y << "  " << min_cost << "  " << block.Num() << "\n";

        block.SetLoc(res_x, res_y);

        min_x = std::min(min_x, res_x + width / 2.0);
        max_x = std::max(max_x, res_x + width / 2.0);

        start_row = int(block.LLY()) - box.bottom;
        end_row = start_row + int(block.Height()) - 1;

        int end_x = int(block.URX());
        for (int i = start_row; i <= end_row; ++i) {
            row_start[i] = end_x;
        }
    }

    double span_x = max_x - min_x;
    if (span_x >= 0 && span_x < 1e-8) return;
    double box_width = box.right - box.left;
    if (span_x < 0) {
        BOOST_LOG_TRIVIAL(info) << span_x << "  " << index_loc_list.size()
                                << "\n";
    }
    DaliExpects(span_x > 0, "Expect span_x > 0!");
    for (auto &pair: index_loc_list) {
        auto &block = block_list[pair.num];
        block.SetCenterX((block.X() - min_x) / span_x * box_width + box.left);
    }

}

double GPSimPL::BlkOverlapArea(Block *node1, Block *node2) {
    bool not_overlap;
    not_overlap =
        ((node1->LLX() >= node2->URX()) || (node1->LLY() >= node2->URY()))
            || ((node2->LLX() >= node1->URX())
                || (node2->LLY() >= node1->URY()));
    if (not_overlap) {
        return 0;
    } else {
        double node1_llx, node1_lly, node1_urx, node1_ury;
        double node2_llx, node2_lly, node2_urx, node2_ury;
        double max_llx, max_lly, min_urx, min_ury;
        double overlap_x, overlap_y, overlap_area;

        node1_llx = node1->LLX();
        node1_lly = node1->LLY();
        node1_urx = node1->URX();
        node1_ury = node1->URY();
        node2_llx = node2->LLX();
        node2_lly = node2->LLY();
        node2_urx = node2->URX();
        node2_ury = node2->URY();

        max_llx = std::max(node1_llx, node2_llx);
        max_lly = std::max(node1_lly, node2_lly);
        min_urx = std::min(node1_urx, node2_urx);
        min_ury = std::min(node1_ury, node2_ury);

        overlap_x = min_urx - max_llx;
        overlap_y = min_ury - max_lly;
        overlap_area = overlap_x * overlap_y;
        return overlap_area;
    }
}

void GPSimPL::PlaceBlkInBoxBisection(BoxBin &box) {
    std::vector<Block> &block_list = circuit_->Blocks();
    /* keep bisect a grid bin until the leaf bin has less than say 2 nodes? */
    size_t max_cell_num_in_box = 10;
    box.cut_direction_x = true;
    std::queue<BoxBin> box_Q;
    box_Q.push(box);
    while (!box_Q.empty()) {
        //BOOST_LOG_TRIVIAL(info)   << " Q.size: " << box_Q.size() << "\n";
        BoxBin &front_box = box_Q.front();
        if (front_box.cell_list.size() > max_cell_num_in_box) {
            // split box and push it to box_Q
            BoxBin box1, box2;
            box1.ur_index = front_box.ur_index;
            box2.ll_index = front_box.ll_index;
            box1.bottom = front_box.bottom;
            box1.left = front_box.left;
            box2.top = front_box.top;
            box2.right = front_box.right;

            int ave_blk_height = std::ceil(GetCircuit()->AveMovBlkHeight());
            //BOOST_LOG_TRIVIAL(info)   << "Average block height: " << ave_blk_height << "  " << GetCircuit()->AveMovBlkHeight() << "\n";
            front_box.cut_direction_x =
                (front_box.top - front_box.bottom > ave_blk_height);
            int cut_line_w = 0; // cut-line for White space
            front_box.update_cut_point_cell_list_low_high_leaf(block_list,
                                                               cut_line_w,
                                                               ave_blk_height);
            if (front_box.cut_direction_x) {
                box1.top = cut_line_w;
                box1.right = front_box.right;

                box2.bottom = cut_line_w;
                box2.left = front_box.left;
            } else {
                box1.top = front_box.top;
                box1.right = cut_line_w;

                box2.bottom = front_box.bottom;
                box2.left = cut_line_w;
            }
            box1.cell_list = front_box.cell_list_low;
            box2.cell_list = front_box.cell_list_high;
            box1.ll_point = front_box.ll_point;
            box2.ur_point = front_box.ur_point;
            box1.ur_point = front_box.cut_ur_point;
            box2.ll_point = front_box.cut_ll_point;
            box1.total_cell_area = front_box.total_cell_area_low;
            box2.total_cell_area = front_box.total_cell_area_high;

            if (!box1.cell_list.empty()) {
                box_Q.push(box1);
            }
            if (!box2.cell_list.empty()) {
                box_Q.push(box2);
            }
        } else {
            // shift cells to the center of the final box
            if (max_cell_num_in_box == 1) {
                Block *cell;
                for (auto &cell_id: front_box.cell_list) {
                    cell = &block_list[cell_id];
                    cell->SetCenterX((front_box.left + front_box.right) / 2.0);
                    cell->SetCenterY((front_box.bottom + front_box.top) / 2.0);
                }
            } else {
                PlaceBlkInBox(front_box);
                //RoughLegalBlkInBox(front_box);
            }
        }
        box_Q.pop();
    }
}

void GPSimPL::UpdateGridBinBlocks(BoxBin &box) {
    /****
   * Update the block list, area, and overfilled state of a box bin if this box is also a grid bin box
   * ****/

    // if this box is not a grid bin, then do nothing
    if (box.ll_index == box.ur_index) {
        int x_index = box.ll_index.x;
        int y_index = box.ll_index.y;
        GridBin &grid_bin = grid_bin_matrix[x_index][y_index];
        if (grid_bin.left == box.left &&
            grid_bin.bottom == box.bottom &&
            grid_bin.right == box.right &&
            grid_bin.top == box.top) {
            grid_bin.cell_list.clear();
            grid_bin.cell_area = 0;
            grid_bin.over_fill = false;

            std::vector<Block> &block_list = circuit_->Blocks();
            for (auto &blk_num: box.cell_list) {
                grid_bin.cell_list.push_back(blk_num);
                grid_bin.cell_area += block_list[blk_num].Area();
            }
        }
    }

}

bool GPSimPL::RecursiveBisectionBlkSpreading() {
    /* keep splitting the biggest box to many small boxes, and keep update the shape of each box and cells should be assigned to the box */

    double wall_time = get_wall_time();

    while (!queue_box_bin.empty()) {
        //std::cout << queue_box_bin.size() << "\n";
        if (queue_box_bin.empty()) break;
        BoxBin &box = queue_box_bin.front();
        // start moving cells to the box, if
        // (a) the box is a grid bin box or a smaller box
        // (b) and with no fixed macros inside
        if (box.ll_index == box.ur_index) {
            //UpdateGridBinBlocks(box);
            if (box.IsContainFixedBlk()) { // if there is a fixed macro inside a box, keep splitting the box
                SplitGridBox(box);
                queue_box_bin.pop();
                continue;
            }
            /* if no terminals inside a box, do cell placement inside the box */
            //PlaceBlkInBoxBisection(box);
            PlaceBlkInBox(box);
            //RoughLegalBlkInBox(box);
        } else {
            SplitBox(box);
        }
        queue_box_bin.pop();
    }

    RecursiveBisectionBlkSpreading_time += get_wall_time() - wall_time;
    return true;
}

void GPSimPL::BackUpBlockLocation() {
    std::vector<Block> &block_list = circuit_->Blocks();
    size_t sz = block_list.size();
    for (size_t i = 0; i < sz; ++i) {
        x_anchor[i] = block_list[i].LLX();
        y_anchor[i] = block_list[i].LLY();
    }
}

void GPSimPL::UpdateAnchorLocation() {
    std::vector<Block> &block_list = circuit_->Blocks();
    size_t sz = block_list.size();

    for (size_t i = 0; i < sz; ++i) {
        double tmp_loc_x = x_anchor[i];
        x_anchor[i] = block_list[i].LLX();
        block_list[i].SetLLX(tmp_loc_x);

        double tmp_loc_y = y_anchor[i];
        y_anchor[i] = block_list[i].LLY();
        block_list[i].SetLLY(tmp_loc_y);
    }

    x_anchor_set = true;
    y_anchor_set = true;
}

void GPSimPL::UpdateAnchorNetWeight() {
    std::vector<Block> &block_list = circuit_->Blocks();
    size_t sz = block_list.size();

    // X direction
    double weight = 0;
    double pin_loc0, pin_loc1;
    double ave_height = circuit_->AveBlkHeight();
    for (size_t i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) continue;
        pin_loc0 = block_list[i].LLX();
        pin_loc1 = x_anchor[i];
        double displacement = std::fabs(pin_loc0 - pin_loc1);
        weight = alpha / (displacement + width_epsilon_);
        x_anchor_weight[i] = weight;
    }

    // Y direction
    weight = 0;
    for (size_t i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) continue;
        pin_loc0 = block_list[i].LLY();
        pin_loc1 = y_anchor[i];
        double displacement = std::fabs(pin_loc0 - pin_loc1);
        weight = alpha / (displacement + width_epsilon_);
        y_anchor_weight[i] = weight;
    }
}

void GPSimPL::BuildProblemWithAnchorX() {
    UpdateMaxMinX();
    if (net_model == 0) {
        BuildProblemB2BX();
    } else if (net_model == 1) {
        BuildProblemStarModelX();
    } else if (net_model == 2) {
        BuildProblemHPWLX();
    } else {
        BuildProblemStarHPWLX();
    }

    double wall_time = get_wall_time();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t sz = block_list.size();

    double weight = 0;
    double pin_loc0, pin_loc1;
    for (size_t i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) continue;
        pin_loc0 = block_list[i].LLX();
        pin_loc1 = x_anchor[i];
        weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
        bx[i] += pin_loc1 * weight;
        if (net_model == 3) {
            SpMat_diag_x[i].valueRef() += weight;
        } else {
            coefficientsx.emplace_back(T(i, i, weight));
        }
    }
    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_x += wall_time;
}

void GPSimPL::BuildProblemWithAnchorY() {
    UpdateMaxMinY();
    if (net_model == 0) {
        BuildProblemB2BY(); // fill A and b
    } else if (net_model == 1) {
        BuildProblemStarModelY();
    } else if (net_model == 2) {
        BuildProblemHPWLY();
    } else {
        BuildProblemStarHPWLY();
    }

    double wall_time = get_wall_time();

    std::vector<Block> &block_list = circuit_->Blocks();
    size_t sz = block_list.size();

    double weight = 0;
    double pin_loc0, pin_loc1;
    for (size_t i = 0; i < sz; ++i) {
        if (block_list[i].IsFixed()) continue;
        pin_loc0 = block_list[i].LLY();
        pin_loc1 = y_anchor[i];
        weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
        by[i] += pin_loc1 * weight;
        if (net_model == 3) {
            SpMat_diag_y[i].valueRef() += weight;
        } else {
            coefficientsy.emplace_back(T(i, i, weight));
        }
    }
    wall_time = get_wall_time() - wall_time;
    tot_triplets_time_y += wall_time;
}

double GPSimPL::QuadraticPlacementWithAnchor(double net_model_update_stop_criterion) {
    //omp_set_nested(1);
    omp_set_dynamic(0);
    int avail_threads_num = omp_get_max_threads();
    //BOOST_LOG_TRIVIAL(info)   << "total threads: " << avail_threads_num << "\n";
    double wall_time = get_wall_time();

    std::vector<Block> &block_list = circuit_->Blocks();

    UpdateAnchorLocation();
    UpdateAnchorAlpha();
    //UpdateAnchorNetWeight();
    BOOST_LOG_TRIVIAL(trace) << "alpha: " << alpha << "\n";

#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
    {
        //BOOST_LOG_TRIVIAL(info)  <<"OpenMP threads, %d\n", omp_get_num_threads());
        if (omp_get_thread_num() == 0) {
            omp_set_num_threads(avail_threads_num / 2);
            BOOST_LOG_TRIVIAL(trace) << "threads in branch x: "
                                     << omp_get_max_threads()
                                     << " Eigen threads: " << Eigen::nbThreads()
                                     << "\n";
            for (size_t i = 0; i < block_list.size(); ++i) {
                vx[i] = block_list[i].LLX();
            }
            std::vector<double> eval_history_x;
            int b2b_update_it_x = 0;
            for (b2b_update_it_x = 0;
                 b2b_update_it_x < b2b_update_max_iteration_;
                 ++b2b_update_it_x) {
                BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
                BuildProblemWithAnchorX();
                double evaluate_result =
                    OptimizeQuadraticMetricX(cg_stop_criterion_);
                eval_history_x.push_back(evaluate_result);
                //BOOST_LOG_TRIVIAL(trace) << "\tIterative net model update, WeightedHPWLX: " << evaluate_result << "\n";
                if (eval_history_x.size() >= 3) {
                    bool is_converge = IsSeriesConverge(eval_history_x,
                                                        3,
                                                        net_model_update_stop_criterion);
                    bool is_oscillate = IsSeriesOscillate(eval_history_x, 5);
                    if (is_converge) {
                        break;
                    }
                    if (is_oscillate) {
                        BOOST_LOG_TRIVIAL(trace)
                            << "Net model update oscillation detected X\n";
                        break;
                    }
                }
            }
            BOOST_LOG_TRIVIAL(trace)
                << "  Optimization summary X, iterations x: " << b2b_update_it_x
                << ", " << eval_history_x << "\n";
            DaliExpects(!eval_history_x.empty(),
                        "Cannot return a valid value because the result is not evaluated in GPSimPL::QuadraticPlacement()!");
            lower_bound_hpwlx_.push_back(eval_history_x.back());
        }
        if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
            omp_set_num_threads(avail_threads_num / 2);
            BOOST_LOG_TRIVIAL(trace) << "threads in branch y: "
                                     << omp_get_max_threads()
                                     << " Eigen threads: " << Eigen::nbThreads()
                                     << "\n";
            for (size_t i = 0; i < block_list.size(); ++i) {
                vy[i] = block_list[i].LLY();
            }
            std::vector<double> eval_history_y;
            int b2b_update_it_y = 0;
            for (b2b_update_it_y = 0;
                 b2b_update_it_y < b2b_update_max_iteration_;
                 ++b2b_update_it_y) {
                BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
                BuildProblemWithAnchorY();
                double evaluate_result =
                    OptimizeQuadraticMetricY(cg_stop_criterion_);
                eval_history_y.push_back(evaluate_result);
                if (eval_history_y.size() >= 3) {
                    bool is_converge = IsSeriesConverge(eval_history_y,
                                                        3,
                                                        net_model_update_stop_criterion);
                    bool is_oscillate = IsSeriesOscillate(eval_history_y, 5);
                    if (is_converge) {
                        break;
                    }
                    if (is_oscillate) {
                        BOOST_LOG_TRIVIAL(trace)
                            << "Net model update oscillation detected Y\n";
                        break;
                    }
                }
            }
            BOOST_LOG_TRIVIAL(trace)
                << "  Optimization summary Y, iterations y: " << b2b_update_it_y
                << ", " << eval_history_y << "\n";
            DaliExpects(!eval_history_y.empty(),
                        "Cannot return a valid value because the result is not evaluated in GPSimPL::QuadraticPlacement()!");
            lower_bound_hpwly_.push_back(eval_history_y.back());
        }
    }

    PullBlockBackToRegion();

    BOOST_LOG_TRIVIAL(debug) << "Quadratic Placement With Anchor Complete\n";

//  for (size_t i = 0; i < 10; ++i) {
//    BOOST_LOG_TRIVIAL(info)   << vx[i] << "  " << vy[i] << "\n";
//  }

    wall_time = get_wall_time() - wall_time;
    tot_cg_time += wall_time;
    //omp_set_nested(0);

    if (is_dump) DumpResult("cg_result_" + std::to_string(cur_iter_) + ".txt");
    return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
}

double GPSimPL::LookAheadLegalization() {
    double cpu_time = get_cpu_time();

    BackUpBlockLocation();
    ClearGridBinFlag();
    UpdateGridBinState();
    UpdateClusterList();
    do {
        UpdateLargestCluster();
        FindMinimumBoxForLargestCluster();
        RecursiveBisectionBlkSpreading();
        //BOOST_LOG_TRIVIAL(info) << "cluster count: " << cluster_set.size() << "\n";
    } while (!cluster_set.empty());

    //LGTetrisEx legalizer_;
    //legalizer_.TakeOver(this);
    //legalizer_.StartPlacement();

    double evaluate_result_x = WeightedHPWLX();
    upper_bound_hpwlx_.push_back(evaluate_result_x);
    double evaluate_result_y = WeightedHPWLY();
    upper_bound_hpwly_.push_back(evaluate_result_y);
    BOOST_LOG_TRIVIAL(debug) << "Look-ahead legalization complete\n";

    cpu_time = get_cpu_time() - cpu_time;
    tot_lal_time += cpu_time;

    if (is_dump) DumpResult("lal_result_" + std::to_string(cur_iter_) + ".txt");
    if (is_dump)
        DumpLookAheadDisplacement("displace_" + std::to_string(cur_iter_), 1);

    BOOST_LOG_TRIVIAL(debug) << "(UpdateGridBinState time: "
                             << UpdateGridBinState_time << "s)\n";
    BOOST_LOG_TRIVIAL(debug) << "(UpdateClusterList time: "
                             << UpdateClusterList_time << "s)\n";
    BOOST_LOG_TRIVIAL(debug) << "(FindMinimumBoxForLargestCluster time: "
                             << FindMinimumBoxForLargestCluster_time
                             << "s)\n";
    BOOST_LOG_TRIVIAL(debug) << "(RecursiveBisectionBlkSpreading time: "
                             << RecursiveBisectionBlkSpreading_time << "s)\n";

    return evaluate_result_x + evaluate_result_y;
}

void GPSimPL::CheckAndShift() {
    if (circuit_->TotFixedBlkCnt() > 0) return;
    /****
   * This method is helpful when a circuit does not have any fixed blocks.
   * In this case, the shift of the whole circuit does not influence HPWL and overlap.
   * But if the circuit is placed close to the right placement boundary, it give very few change to legalizer_ if cells close
   * to the right boundary need to find different locations.
   *
   * 1. Find the leftmost, rightmost, topmost, bottommost cell edges
   * 2. Calculate the total margin in x direction and y direction
   * 3. Evenly assign the margin to each side
   * ****/

    double left_most = INT_MAX;
    double right_most = INT_MIN;
    double bottom_most = INT_MAX;
    double top_most = INT_MIN;

    for (auto &blk: circuit_->Blocks()) {
        left_most = std::min(left_most, blk.LLX());
        right_most = std::max(right_most, blk.URX());
        bottom_most = std::min(bottom_most, blk.LLY());
        top_most = std::max(top_most, blk.URY());
    }

    double margin_x = (right_ - left_) - (right_most - left_most);
    double margin_y = (top_ - bottom_) - (top_most - bottom_most);

    double delta_x = left_ + margin_x / 10 - left_most;
    double delta_y = bottom_ + margin_y / 2 - bottom_most;

    for (auto &blk: circuit_->Blocks()) {
        blk.IncreaseX(delta_x);
        blk.IncreaseY(delta_y);
    }
}

bool GPSimPL::IsPlacementConverge() {
    /****
   * Returns true of false indicating the convergence of the global placement.
   * Stopping criteria (SimPL, option 1):
   *    (a). the gap is reduced to 25% of the gap in the tenth iteration and upper-bound solution stops improving
   *    (b). the gap is smaller than 10% of the gap in the tenth iteration
   * Stopping criteria (POLAR, option 2):
   *    the gap between lower bound wirelength and upper bound wirelength is less than 8%
   * ****/

    bool res = false;
    if (convergence_criteria_ == 1) {
        // (a) and (b) requires at least 10 iterations
        if (lower_bound_hpwl_.size() <= 10) {
            res = false;
        } else {
            double tenth_gap = upper_bound_hpwl_[9] - lower_bound_hpwl_[9];
            double
                last_gap = upper_bound_hpwl_.back() - lower_bound_hpwl_.back();
            double gap_ratio = last_gap / tenth_gap;
            if (gap_ratio < 0.1) { // (a)
                res = true;
            } else if (gap_ratio < 0.25) { // (b)
                res = IsSeriesConverge(upper_bound_hpwl_,
                                       3,
                                       simpl_LAL_converge_criterion_);
            } else {
                res = false;
            }
        }
    } else if (convergence_criteria_ == 2) {
        if (lower_bound_hpwl_.empty()) {
            res = false;
        } else {
            double lower_bound = lower_bound_hpwl_.back();
            double upper_bound = upper_bound_hpwl_.back();
            res = (lower_bound < upper_bound)
                && (upper_bound / lower_bound - 1 < polar_converge_criterion_);
        }
    } else {
        DaliExpects(false, "Unknown Convergence Criteria!");
    }

    return res;
}

bool GPSimPL::StartPlacement() {
    double wall_time = get_wall_time();
    double cpu_time = get_cpu_time();

    BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n";
    BOOST_LOG_TRIVIAL(info) << "Start global placement\n";

    SanityCheck();
    CGInit();
    LALInit();
    //BlockLocCenterInit();
    BlockLocRandomInit();
    if (net_model == 3) {
        DriverLoadPairInit();
    }

    if (NetList()->empty()) {
        BOOST_LOG_TRIVIAL(info) << "Net list empty\n";
        BOOST_LOG_TRIVIAL(info)
            << "\033[0;36m Global Placement complete\033[0m\n";
        return true;
    }

    // initial placement
    BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";
    double eval_res = QuadraticPlacement(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);
    eval_res = LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);
    BOOST_LOG_TRIVIAL(info) << "It " << cur_iter_ << ": \t"
                            << std::scientific << std::setprecision(4)
                            << lower_bound_hpwl_.back() << " "
                            << upper_bound_hpwl_.back() << "\n";

    for (cur_iter_ = 1; cur_iter_ < max_iter_; ++cur_iter_) {
        BOOST_LOG_TRIVIAL(debug)
            << "----------------------------------------------\n";
        BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";

        eval_res =
            QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
        lower_bound_hpwl_.push_back(eval_res);

        eval_res = LookAheadLegalization();
        upper_bound_hpwl_.push_back(eval_res);

        BOOST_LOG_TRIVIAL(info) << "It " << cur_iter_ << ": \t"
                                << std::scientific << std::setprecision(4)
                                << lower_bound_hpwl_.back() << " "
                                << upper_bound_hpwl_.back() << "\n";

        if (IsPlacementConverge()) { // if HPWL converges
            BOOST_LOG_TRIVIAL(info)
                << "Iterative look-ahead legalization complete\n";
            BOOST_LOG_TRIVIAL(info) << "Total number of iteration: "
                                    << cur_iter_ + 1 << "\n";
            break;
        }
    }
    int num_it = lower_bound_hpwl_.size();
    BOOST_LOG_TRIVIAL(info) << "Random init: " << init_hpwl_ << "\n";
    BOOST_LOG_TRIVIAL(info) << "Lower bound: " << lower_bound_hpwl_ << "\n";
    BOOST_LOG_TRIVIAL(info) << "Upper bound: " << upper_bound_hpwl_ << "\n";

    BOOST_LOG_TRIVIAL(info) << "\033[0;36m Global Placement complete\033[0m\n";
    BOOST_LOG_TRIVIAL(info) << "(cg time: " << tot_cg_time << "s, lal time: "
                            << tot_lal_time << "s)\n";
    BOOST_LOG_TRIVIAL(info) << "total triplets time: "
                            << tot_triplets_time_x << "s, "
                            << tot_triplets_time_y << "s, "
                            << tot_triplets_time_x + tot_triplets_time_y
                            << "s\n";
    BOOST_LOG_TRIVIAL(info) << "total matrix from triplets time: "
                            << tot_matrix_from_triplets_x << "s, "
                            << tot_matrix_from_triplets_y << "s, "
                            << tot_matrix_from_triplets_x
                                + tot_matrix_from_triplets_y << "s\n";
    BOOST_LOG_TRIVIAL(info) << "total cg solver time: "
                            << tot_cg_solver_time_x << "s, "
                            << tot_cg_solver_time_y << "s, "
                            << tot_cg_solver_time_x + tot_cg_solver_time_y
                            << "s\n";
    BOOST_LOG_TRIVIAL(info) << "total loc update time: "
                            << tot_loc_update_time_x << "s, "
                            << tot_loc_update_time_y << "s, "
                            << tot_loc_update_time_x + tot_loc_update_time_y
                            << "s\n";
    double tot_time_x =
        tot_triplets_time_x + tot_matrix_from_triplets_x + tot_cg_solver_time_x
            + tot_loc_update_time_x;
    double tot_time_y =
        tot_triplets_time_y + tot_matrix_from_triplets_y + tot_cg_solver_time_y
            + tot_loc_update_time_y;
    BOOST_LOG_TRIVIAL(info) << "total x/y time: "
                            << tot_time_x << "s, "
                            << tot_time_y << "s, "
                            << tot_time_x + tot_time_y << "s\n";
    LALClose();
    //CheckAndShift();
    UpdateMovableBlkPlacementStatus();
    ReportHPWL();

    wall_time = get_wall_time() - wall_time;
    cpu_time = get_cpu_time() - cpu_time;
    BOOST_LOG_TRIVIAL(info) << "(wall time: " << wall_time << "s, cpu time: "
                            << cpu_time << "s)\n";
    ReportMemory();

    return true;
}

void GPSimPL::DumpResult(std::string const &name_of_file) {
    //UpdateGridBinState();
    static int counter = 0;
    //BOOST_LOG_TRIVIAL(info)   << "DumpNum:" << counter << "\n";
    circuit_->GenMATLABTable(name_of_file);
    //write_not_all_terminal_grid_bins("grid_bin_not_all_terminal" + std::to_string(counter) + ".txt");
    //write_overfill_grid_bins("grid_bin_overfill" + std::to_string(counter) + ".txt");
    ++counter;
}

void GPSimPL::DumpLookAheadDisplacement(
    std::string const &base_name,
    int mode
) {
    /****
   * Dump the displacement of all cells during look ahead legalization.
   * @param mode:
   *    if 0: dump displacement for MATLAB visualization (quiver/vector plot)
   *    if 1: dump displacement for distribution analysis, unit in average cell height
   *    if 2: dump both
   * ****/

    if (mode < 0 || mode > 2) return;
    if (mode == 0 || mode == 2) {
        std::string name_of_file = base_name + "quiver.txt";
        std::ofstream ost(name_of_file.c_str());
        DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

        DaliExpects(circuit_ != nullptr,
                    "Set input circuit before starting anything GPSimPL::DumpLookAheadDisplacement()");
        std::vector<Block> &block_list = circuit_->Blocks();
        int sz = circuit_->design().RealBlkCnt();
        for (int i = 0; i < sz; ++i) {
            double x = x_anchor[i] + block_list[i].Width() / 2.0;
            double y = y_anchor[i] + +block_list[i].Height() / 2.0;
            double u = block_list[i].X() - x;
            double v = block_list[i].Y() - y;
            ost << x << "\t" << y << "\t";
            ost << u << "\t" << v << "\t";
            ost << "\n";
        }
        ost.close();
    }

    if (mode == 1 || mode == 2) {
        std::string name_of_file = base_name + "hist.txt";
        std::ofstream ost(name_of_file.c_str());
        DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

        DaliExpects(circuit_ != nullptr,
                    "Set input circuit before starting anything GPSimPL::DumpLookAheadDisplacement()");
        double ave_height = circuit_->AveMovBlkHeight();
        std::vector<Block> &block_list = circuit_->Blocks();
        int sz = circuit_->design().RealBlkCnt();
        for (int i = 0; i < sz; ++i) {
            double x = x_anchor[i] + block_list[i].Width() / 2.0;
            double y = y_anchor[i] + +block_list[i].Height() / 2.0;
            double u = block_list[i].X() - x;
            double v = block_list[i].Y() - y;
            double displacement = sqrt(u * u + v * v);
            ost << displacement / ave_height << "\n";
        }
        ost.close();
    }
}

void GPSimPL::DrawBlockNetList(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open input file " + name_of_file);
    ost << RegionLeft() << " " << RegionBottom() << " "
        << RegionRight() - RegionLeft() << " "
        << RegionTop() - RegionBottom() << "\n";
    std::vector<Block> &block_list = circuit_->Blocks();
    for (auto &block: block_list) {
        ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " "
            << block.Height() << "\n";
    }
    ost.close();
}

void GPSimPL::write_all_terminal_grid_bins(std::string const &name_of_file) {
    /* this is a member function for testing, print grid bins where the flag "all_terminal" is true */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            double low_x, low_y, width, height;
            width = bin.right - bin.left;
            height = bin.top - bin.bottom;
            low_x = bin.left;
            low_y = bin.bottom;
            int N = 3;
            double step_x = width / N, step_y = height / N;
            if (bin.IsAllFixedBlk()) {
                for (int j = 0; j < N; j++) {
                    ost << low_x << "\t" << low_y + j * step_y << "\n";
                    ost << low_x + width << "\t" << low_y + j * step_y << "\n";
                }
                for (int j = 0; j < N; j++) {
                    ost << low_x + j * step_x << "\t" << low_y << "\n";
                    ost << low_x + j * step_x << "\t" << low_y + height << "\n";
                }
            }
        }
    }
    ost.close();
}

void GPSimPL::write_not_all_terminal_grid_bins(std::string const &name_of_file) {
    /* this is a member function for testing, print grid bins where the flag "all_terminal" is false */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            double low_x, low_y, width, height;
            width = bin.right - bin.left;
            height = bin.top - bin.bottom;
            low_x = bin.left;
            low_y = bin.bottom;
            int N = 3;
            double step_x = width / N, step_y = height / N;
            if (!bin.IsAllFixedBlk()) {
                for (int j = 0; j < N; j++) {
                    ost << low_x << "\t" << low_y + j * step_y << "\n";
                    ost << low_x + width << "\t" << low_y + j * step_y << "\n";
                }
                for (int j = 0; j < N; j++) {
                    ost << low_x + j * step_x << "\t" << low_y << "\n";
                    ost << low_x + j * step_x << "\t" << low_y + height << "\n";
                }
            }
        }
    }
    ost.close();
}

void GPSimPL::write_overfill_grid_bins(std::string const &name_of_file) {
    /* this is a member function for testing, print grid bins where the flag "over_fill" is true */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            double low_x, low_y, width, height;
            width = bin.right - bin.left;
            height = bin.top - bin.bottom;
            low_x = bin.left;
            low_y = bin.bottom;
            int N = 10;
            double step_x = width / N, step_y = height / N;
            if (bin.OverFill()) {
                for (int j = 0; j < N; j++) {
                    ost << low_x << "\t" << low_y + j * step_y << "\n";
                    ost << low_x + width << "\t" << low_y + j * step_y << "\n";
                }
                for (int j = 0; j < N; j++) {
                    ost << low_x + j * step_x << "\t" << low_y << "\n";
                    ost << low_x + j * step_x << "\t" << low_y + height << "\n";
                }
            }
        }
    }
    ost.close();
}

void GPSimPL::write_not_overfill_grid_bins(std::string const &name_of_file) {
    /* this is a member function for testing, print grid bins where the flag "over_fill" is false */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    for (auto &bin_column: grid_bin_matrix) {
        for (auto &bin: bin_column) {
            double low_x, low_y, width, height;
            width = bin.right - bin.left;
            height = bin.top - bin.bottom;
            low_x = bin.left;
            low_y = bin.bottom;
            int N = 10;
            double step_x = width / N, step_y = height / N;
            if (!bin.OverFill()) {
                for (int j = 0; j < N; j++) {
                    ost << low_x << "\t" << low_y + j * step_y << "\n";
                    ost << low_x + width << "\t" << low_y + j * step_y << "\n";
                }
                for (int j = 0; j < N; j++) {
                    ost << low_x + j * step_x << "\t" << low_y << "\n";
                    ost << low_x + j * step_x << "\t" << low_y + height << "\n";
                }
            }
        }
    }
    ost.close();
}

void GPSimPL::write_first_n_bin_cluster(std::string const &name_of_file,
                                        size_t n) {
    /* this is a member function for testing, print the first n over_filled clusters */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    auto it = cluster_set.begin();
    for (size_t i = 0; i < n; ++i, --it) {
        for (auto &index: it->bin_set) {
            double low_x, low_y, width, height;
            GridBin *GridBin = &grid_bin_matrix[index.x][index.y];
            width = GridBin->right - GridBin->left;
            height = GridBin->top - GridBin->bottom;
            low_x = GridBin->left;
            low_y = GridBin->bottom;
            int step = 40;
            if (GridBin->OverFill()) {
                for (int j = 0; j < height; j += step) {
                    ost << low_x << "\t" << low_y + j << "\n";
                    ost << low_x + width << "\t" << low_y + j << "\n";
                }
                for (int j = 0; j < width; j += step) {
                    ost << low_x + j << "\t" << low_y << "\n";
                    ost << low_x + j << "\t" << low_y + height << "\n";
                }
            }
        }
    }
    ost.close();
}

void GPSimPL::write_first_bin_cluster(std::string const &name_of_file) {
    /* this is a member function for testing, print the first one over_filled clusters */
    write_first_n_bin_cluster(name_of_file, 1);
}

void GPSimPL::write_all_bin_cluster(const std::string &name_of_file) {
    /* this is a member function for testing, print all over_filled clusters */
    write_first_n_bin_cluster(name_of_file, cluster_set.size());
}

void GPSimPL::write_first_box(std::string const &name_of_file) {
    /* this is a member function for testing, print the first n over_filled clusters */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    double low_x, low_y, width, height;
    BoxBin *R = &queue_box_bin.front();
    width = (R->ur_index.x - R->ll_index.x + 1) * grid_bin_width;
    height = (R->ur_index.y - R->ll_index.y + 1) * grid_bin_height;
    low_x = R->ll_index.x * grid_bin_width + RegionLeft();
    low_y = R->ll_index.y * grid_bin_height + RegionBottom();
    int step = 20;
    for (int j = 0; j < height; j += step) {
        ost << low_x << "\t" << low_y + j << "\n";
        ost << low_x + width << "\t" << low_y + j << "\n";
    }
    for (int j = 0; j < width; j += step) {
        ost << low_x + j << "\t" << low_y << "\n";
        ost << low_x + j << "\t" << low_y + height << "\n";
    }
    ost.close();
}

void GPSimPL::write_first_box_cell_bounding(std::string const &name_of_file) {
    /* this is a member function for testing, print the bounding box of cells in which all cells should be placed into corresponding boxes */
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
    double low_x, low_y, width, height;
    BoxBin *R = &queue_box_bin.front();
    width = R->ur_point.x - R->ll_point.x;
    height = R->ur_point.y - R->ll_point.y;
    low_x = R->ll_point.x;
    low_y = R->ll_point.y;
    int step = 30;
    for (int j = 0; j < height; j += step) {
        ost << low_x << "\t" << low_y + j << "\n";
        ost << low_x + width << "\t" << low_y + j << "\n";
    }
    for (int j = 0; j < width; j += step) {
        ost << low_x + j << "\t" << low_y << "\n";
        ost << low_x + j << "\t" << low_y + height << "\n";
    }
    ost.close();
}

bool GPSimPL::IsSeriesConverge(std::vector<double> &data,
                               int window_size,
                               double tolerance) {
    int sz = (int) data.size();
    if (sz < window_size) {
        return false;
    }
    double max_val = DBL_MIN;
    double min_val = DBL_MAX;
    for (int i = 0; i < window_size; ++i) {
        max_val = std::max(max_val, data[sz - 1 - i]);
        min_val = std::min(min_val, data[sz - 1 - i]);
    }
    DaliExpects(max_val >= 0 && min_val >= 0,
                "Do not support negative data series!");
    if (max_val < 1e-10 && min_val <= 1e-10) {
        return true;
    }
    double ratio = max_val / min_val - 1;
    return ratio < tolerance;
}

bool GPSimPL::IsSeriesOscillate(std::vector<double> &data, int length) {
    /****
   * Returns if the given series of data is oscillating or not.
   * We will only look at the last several data points @param length.
   * ****/

    // if the given length is too short, we cannot know whether it is oscillating or not.
    if (length < 3) return false;

    // if the given data series is short than the length, we cannot know whether it is oscillating or not.
    int sz = (int) data.size();
    if (sz < length) {
        return false;
    }

    // this vector keeps track of the increasing trend (true) and descreasing trend (false).
    std::vector<bool> trend(length - 1, false);
    for (int i = 0; i < length - 1; ++i) {
        trend[i] = data[sz - 1 - i] > data[sz - 2 - i];
    }
    std::reverse(trend.begin(), trend.end());

    bool is_oscillate = true;
    for (int i = 0; i < length - 2; ++i) {
        if (trend[i] == trend[i + 1]) {
            is_oscillate = false;
            break;
        }
    }

    return is_oscillate;
}

}
