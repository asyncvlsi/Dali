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
#include "global_placer.h"

#include <cmath>
#include <omp.h>

#include <algorithm>

#include "dali/common/helper.h"
#include "dali/common/logging.h"

namespace dali {

/****
 * @brief Set the maximum number of iterations
 *
 * @param max_iter: maximum number of iterations
 */
void GlobalPlacer::SetMaxIteration(int max_iter) {
  DaliExpects(max_iter >= 0, "negative number of iterations?");
  max_iter_ = max_iter;
}

/****
 * @brief Load a configuration file for this placer
 *
 * @param config_file: name of the configuration file
 */
void GlobalPlacer::LoadConf(std::string const &config_file) {
  config_read(config_file.c_str());
  std::string variable;

  variable = "dali.global_placer.cg_iteration_";
  if (config_exists(variable.c_str()) == 1) {
    cg_iteration_ = config_get_int(variable.c_str());
  }
  variable = "dali.global_placer.cg_iteration_max_num_";
  if (config_exists(variable.c_str()) == 1) {
    cg_iteration_max_num_ = config_get_int(variable.c_str());
  }
  variable = "dali.global_placer.cg_stop_criterion_";
  if (config_exists(variable.c_str()) == 1) {
    cg_stop_criterion_ = config_get_real(variable.c_str());
  }
  variable = "dali.global_placer.net_model_update_stop_criterion_";
  if (config_exists(variable.c_str()) == 1) {
    net_model_update_stop_criterion_ = config_get_real(variable.c_str());
  }

  variable = "dali.global_placer.b2b_update_max_iteration_";
  if (config_exists(variable.c_str()) == 1) {
    b2b_update_max_iteration_ = config_get_int(variable.c_str());
  }
  variable = "dali.global_placer.max_iter_";
  if (config_exists(variable.c_str()) == 1) {
    max_iter_ = config_get_int(variable.c_str());
  }
  variable = "dali.global_placer.number_of_cell_in_bin_";
  if (config_exists(variable.c_str()) == 1) {
    number_of_cell_in_bin_ = config_get_int(variable.c_str());
  }
  variable = "dali.global_placer.net_ignore_threshold_";
  if (config_exists(variable.c_str()) == 1) {
    net_ignore_threshold_ = config_get_int(variable.c_str());
  }

  variable = "dali.global_placer.simpl_LAL_converge_criterion_";
  if (config_exists(variable.c_str()) == 1) {
    simpl_LAL_converge_criterion_ = config_get_real(variable.c_str());
  }
  variable = "dali.global_placer.polar_converge_criterion_";
  if (config_exists(variable.c_str()) == 1) {
    polar_converge_criterion_ = config_get_real(variable.c_str());
  }
  variable = "dali.global_placer.convergence_criteria_";
  if (config_exists(variable.c_str()) == 1) {
    convergence_criteria_ = config_get_int(variable.c_str());
  }
}

/****
 * @brief During quadratic placement, net weights are computed by dividing the
 * distance between two pins. To improve numerical stability, a small number is
 * added to this distance.
 *
 * In our implementation, this small number is around the average movable cell
 * width for x and height for y.
 */
void GlobalPlacer::UpdateEpsilon() {
  width_epsilon_ = ckt_ptr_->AveMovBlkWidth() * epsilon_factor_;
  height_epsilon_ = ckt_ptr_->AveMovBlkHeight() * epsilon_factor_;
}

/****
 * @brief Initialize the location of blocks uniformly across the placement region
 */
void GlobalPlacer::InitializeBlockLocationUniform() {
  BlockLocationInitialization_(0, -1.0);
}

/****
 * @brief Initialize the location of blocks around the center of the placement
 * region using normal distribution
 */
void GlobalPlacer::InitializeBlockLocationNormal(double std_dev) {
  BlockLocationInitialization_(1, std_dev);
}

/****
 * @brief Initialize variables for the conjugate gradient linear solver
 */
void GlobalPlacer::InitializeHpwlOptimizer() {
  // set a small value for net weight dividend to improve numerical stability
  UpdateEpsilon();

  // initialize containers to store HPWL after each iteration
  lower_bound_hpwlx_.clear();
  lower_bound_hpwly_.clear();
  lower_bound_hpwl_.clear();

  size_t sz = ckt_ptr_->Blocks().size();
  ADx.resize(sz);
  ADy.resize(sz);
  Ax_row_size.assign(sz, 0);
  Ax_row_size.assign(sz, 0);

  EgId eigen_sz = static_cast<EgId>(ckt_ptr_->Blocks().size());
  vx.resize(eigen_sz);
  vy.resize(eigen_sz);
  bx.resize(eigen_sz);
  by.resize(eigen_sz);
  Ax.resize(eigen_sz, eigen_sz);
  Ay.resize(eigen_sz, eigen_sz);
  x_anchor.resize(eigen_sz);
  y_anchor.resize(eigen_sz);
  x_anchor_weight.resize(eigen_sz);
  y_anchor_weight.resize(eigen_sz);

  cg_x_.setMaxIterations(cg_iteration_);
  cg_x_.setTolerance(cg_tolerance_);
  cg_y_.setMaxIterations(cg_iteration_);
  cg_y_.setTolerance(cg_tolerance_);

  size_t coefficient_size = 0;
  auto &nets = ckt_ptr_->Nets();
  for (auto &net : nets) {
    size_t net_sz = net.PinCnt();
    // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero entries for the matrix
    if (net_sz > 1) {
      coefficient_size += (2 * (net_sz - 2) + 1) * 4;
    }
  }
  // this is to reserve space for anchor, because each block may need an anchor
  coefficient_size += sz;
  // this is to reserve space for anchor in the center of the placement region
  coefficient_size += sz;
  coefficients_x_.reserve(coefficient_size);
  Ax.reserve(static_cast<EgId>(coefficient_size));
  coefficients_y_.reserve(coefficient_size);
  Ay.reserve(static_cast<EgId>(coefficient_size));
}

void GlobalPlacer::DecomposeNetsToBlkPairs() {
  // for each net, we decompose it, and enumerate all driver-load pair
  std::vector<Net> &net_list = ckt_ptr_->Nets();
  for (auto &net : net_list) {
    //BOOST_LOG_TRIVIAL(info)   << net.NameStr() << "\n";
    int sz = static_cast<int>(net.BlockPins().size());
    int driver_index = -1;
    // find if there is a driver pin in this net
    // if a net contains a unplaced IOPin, then there might be no driver pin, this case is ignored for now
    // we also assume there is only one driver pin
    for (int i = 0; i < sz; ++i) {
      BlkPinPair &blk_pin_pair = net.BlockPins()[i];
      if (!(blk_pin_pair.PinPtr()->IsInput())) {
        driver_index = i;
        break;
      }
    }
    if (driver_index == -1) continue;

    int driver_blk_num = net.BlockPins()[driver_index].BlkId();
    for (EgId i = 0; i < sz; ++i) {
      if (i == driver_index) continue;
      int load_blk_num = net.BlockPins()[i].BlkId();
      if (driver_blk_num == load_blk_num) continue;
      int num0 = std::max(driver_blk_num, load_blk_num);
      int num1 = std::min(driver_blk_num, load_blk_num);
      std::pair<int, int> key = std::make_pair(num0, num1);
      if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
        int val = int(blk_pair_net_list_.size());
        blk_pair_map_.insert({key, val});
        blk_pair_net_list_.emplace_back(num0, num1);
      }
      EgId pair_index = blk_pair_map_[key];
      EgId load_index = i;
      blk_pair_net_list_[pair_index].edges.emplace_back(
          &net, driver_index, load_index
      );
    }
  }
}

void GlobalPlacer::InitializeDriverLoadPairs() {
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int sz = static_cast<int>(blocks.size());

  pair_connect.resize(sz);
  for (auto &blk_pair : blk_pair_net_list_) {
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
    std::sort(
        pair_connect[i].begin(),
        pair_connect[i].end(),
        [](const BlkPairNets *blk_pair0,
           const BlkPairNets *blk_pair1) {
          if (blk_pair0->blk_num0 == blk_pair1->blk_num0) {
            return blk_pair0->blk_num1 < blk_pair1->blk_num1;
          } else {
            return blk_pair0->blk_num0 < blk_pair1->blk_num0;
          }
        }
    );
  }

  std::vector<int> row_size(sz, 1);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (num0 == num1) continue;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        ++row_size[i];
      }
    }
  }
  Ax.reserve(row_size);
  SpMat_diag_x.resize(sz);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        int col;
        if (num0 == i) {
          col = num1;
        } else {
          col = num0;
        }
        Ax.insertBackUncompressed(i, col) = 0;
      }
    }
    PairEgId key = std::make_pair(0, 0);
    for (SpMat::InnerIterator it(Ax, i); it; ++it) {
      EgId row = it.row();
      EgId col = it.col();
      if (row == col) {
        SpMat_diag_x[i] = it;
        continue;
      }
      key.first = std::max(row, col);
      key.second = std::min(row, col);
      if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
        BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
        DaliExpects(
            false,
            "Cannot find block pair in the database, something wrong happens\n"
        );
      }
      EgId pair_index = blk_pair_map_[key];
      if (blk_pair_net_list_[pair_index].blk_num0 == row) {
        blk_pair_net_list_[pair_index].it01x = it;
      } else {
        blk_pair_net_list_[pair_index].it10x = it;
      }
    }
  }

  Ay.reserve(row_size);
  SpMat_diag_y.resize(sz);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        int col;
        if (num0 == i) {
          col = num1;
        } else {
          col = num0;
        }
        Ay.insertBackUncompressed(i, col) = 0;
      }
    }
    PairEgId key = std::make_pair(0, 0);
    for (SpMat::InnerIterator it(Ay, i); it; ++it) {
      EgId row = it.row();
      EgId col = it.col();
      if (row == col) {
        SpMat_diag_y[i] = it;
        continue;
      }
      key.first = std::max(row, col);
      key.second = std::min(row, col);
      if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
        BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
        DaliExpects(
            false,
            "Cannot find block pair in the database, something wrong happens\n"
        );
      }
      EgId pair_index = blk_pair_map_[key];
      if (blk_pair_net_list_[pair_index].blk_num0 == row) {
        blk_pair_net_list_[pair_index].it01y = it;
      } else {
        blk_pair_net_list_[pair_index].it10y = it;
      }
    }
  }
}

void GlobalPlacer::UpdateMaxMinX() {
  std::vector<Net> &net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdX();
  }
}

void GlobalPlacer::UpdateMaxMinY() {
  std::vector<Net> &net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdY();
  }
}

void GlobalPlacer::BuildProblemB2BX() {
  double wall_time = get_wall_time();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);
  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdX();
    int max_pin_index = net.MaxBlkPinIdX();
    int min_pin_index = net.MinBlkPinIdX();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetX();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetX();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsX();
      bool is_movable = pair.BlkPtr()->IsMovable();
      double offset = pair.OffsetX();

      if (blk_num != blk_num_max) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + width_epsilon_);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          bx[blk_num_max] += (pin_loc - offset_max) * weight;
          coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable && !is_movable_max) {
          bx[blk_num] += (pin_loc_max - offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
          coefficients_x_.emplace_back(blk_num, blk_num_max, -weight);
          coefficients_x_.emplace_back(blk_num_max, blk_num, -weight);
          double offset_diff = (offset_max - offset) * weight;
          bx[blk_num] += offset_diff;
          bx[blk_num_max] -= offset_diff;
        }
      }

      if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + width_epsilon_);
        //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          bx[blk_num_min] += (pin_loc - offset_min) * weight;
          coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable && !is_movable_min) {
          bx[blk_num] += (pin_loc_min - offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
          coefficients_x_.emplace_back(blk_num, blk_num_min, -weight);
          coefficients_x_.emplace_back(blk_num_min, blk_num, -weight);
          double offset_diff = (offset_min - offset) * weight;
          bx[blk_num] += offset_diff;
          bx[blk_num_min] -= offset_diff;
        }
      }
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = blocks[i].LLX();
    } else {
      if (blocks[i].LLX() < RegionLeft()
          || blocks[i].URX() > RegionRight()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}

void GlobalPlacer::BuildProblemB2BY() {
  double wall_time = get_wall_time();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);
  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdY();
    int max_pin_index = net.MaxBlkPinIdY();
    int min_pin_index = net.MinBlkPinIdY();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetY();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetY();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsY();
      bool is_movable = pair.BlkPtr()->IsMovable();
      double offset = pair.OffsetY();

      if (blk_num != blk_num_max) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + height_epsilon_);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          by[blk_num_max] += (pin_loc - offset_max) * weight;
          coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable && !is_movable_max) {
          by[blk_num] += (pin_loc_max - offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
          coefficients_y_.emplace_back(blk_num, blk_num_max, -weight);
          coefficients_y_.emplace_back(blk_num_max, blk_num, -weight);
          double offset_diff = (offset_max - offset) * weight;
          by[blk_num] += offset_diff;
          by[blk_num_max] -= offset_diff;
        }
      }

      if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + height_epsilon_);
        //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          by[blk_num_min] += (pin_loc - offset_min) * weight;
          coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable && !is_movable_min) {
          by[blk_num] += (pin_loc_min - offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
          coefficients_y_.emplace_back(blk_num, blk_num_min, -weight);
          coefficients_y_.emplace_back(blk_num_min, blk_num, -weight);
          double offset_diff = (offset_min - offset) * weight;
          by[blk_num] += offset_diff;
          by[blk_num_min] -= offset_diff;
        }
      }

    }
  }
  // add the diagonal non-zero element for fixed blocks
  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = blocks[i].LLY();
    } else {
      if (blocks[i].LLY() < RegionBottom()
          || blocks[i].URY() > RegionTop()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: y coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
}

void GlobalPlacer::BuildProblemStarModelX() {
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();

    // assuming the 0-th pin in the net is the driver pin
    int driver_blk_num = net.BlockPins()[0].BlkId();
    double driver_pin_loc = net.BlockPins()[0].AbsX();
    bool driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
    double driver_offset = net.BlockPins()[0].OffsetX();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsX();
      bool is_movable = pair.BlkPtr()->IsMovable();

      if (blk_num != driver_blk_num) {
        double distance = std::fabs(pin_loc - driver_pin_loc);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + width_epsilon_) * weight_adjust;
        double weight = inv_p / (distance + width_epsilon_);
        if (!is_movable && driver_is_movable) {
          bx[0] += (pin_loc - driver_offset) * weight;
          coefficients_x_.emplace_back(driver_blk_num, driver_blk_num, weight);
        } else if (is_movable && !driver_is_movable) {
          bx[blk_num] += (driver_pin_loc - driver_offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && driver_is_movable) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(driver_blk_num, driver_blk_num, weight);
          coefficients_x_.emplace_back(blk_num, driver_blk_num, -weight);
          coefficients_x_.emplace_back(driver_blk_num, blk_num, -weight);
          double offset_diff = (driver_offset - pair.OffsetX()) * weight;
          bx[blk_num] += offset_diff;
          bx[driver_blk_num] -= offset_diff;
        }
      }
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = block_list[i].LLX();
    } else {
      if (block_list[i].LLX() < RegionLeft()
          || block_list[i].URX() > RegionRight()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}

void GlobalPlacer::BuildProblemStarModelY() {
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();

    // assuming the 0-th pin in the net is the driver pin
    int driver_blk_num = net.BlockPins()[0].BlkId();
    double driver_pin_loc = net.BlockPins()[0].AbsY();
    bool driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
    double driver_offset = net.BlockPins()[0].OffsetY();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsY();
      bool is_movable = pair.BlkPtr()->IsMovable();

      if (blk_num != driver_blk_num) {
        double distance = std::fabs(pin_loc - driver_pin_loc);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + height_epsilon_) * weight_adjust;
        double weight = inv_p / (distance + height_epsilon_);
        if (!is_movable && driver_is_movable) {
          by[0] += (pin_loc - driver_offset) * weight;
          coefficients_y_.emplace_back(driver_blk_num, driver_blk_num, weight);
        } else if (is_movable && !driver_is_movable) {
          by[blk_num] += (driver_pin_loc - driver_offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && driver_is_movable) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(driver_blk_num, driver_blk_num, weight);
          coefficients_y_.emplace_back(blk_num, driver_blk_num, -weight);
          coefficients_y_.emplace_back(driver_blk_num, blk_num, -weight);
          double offset_diff = (driver_offset - pair.OffsetY()) * weight;
          by[blk_num] += offset_diff;
          by[driver_blk_num] -= offset_diff;
        }
      }
    }
  }

  // add the diagonal non-zero element for fixed blocks
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = block_list[i].LLY();
    } else {
      if (block_list[i].LLY() < RegionBottom()
          || block_list[i].URY() > RegionTop()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: y coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
}

void GlobalPlacer::BuildProblemHPWLX() {
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdX();
    int max_pin_index = net.MaxBlkPinIdX();
    int min_pin_index = net.MinBlkPinIdX();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetX();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetX();

    double distance = std::fabs(pin_loc_min - pin_loc_max);
    //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
    //weight = inv_p / (distance + width_epsilon_) * weight_adjust;
    double weight = inv_p / (distance + width_epsilon_);
    if (!is_movable_min && is_movable_max) {
      bx[blk_num_max] += (pin_loc_min - offset_max) * weight;
      coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
    } else if (is_movable_min && !is_movable_max) {
      bx[blk_num_min] += (pin_loc_max - offset_min) * weight;
      coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
    } else if (is_movable_min && is_movable_max) {
      coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
      coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
      coefficients_x_.emplace_back(blk_num_min, blk_num_max, -weight);
      coefficients_x_.emplace_back(blk_num_max, blk_num_min, -weight);
      double offset_diff = (offset_max - offset_min) * weight;
      bx[blk_num_min] += offset_diff;
      bx[blk_num_max] -= offset_diff;
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = block_list[i].LLX();
    } else {
      if (block_list[i].LLX() < RegionLeft()
          || block_list[i].URX() > RegionRight()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}

void GlobalPlacer::BuildProblemHPWLY() {
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdY();
    int max_pin_index = net.MaxBlkPinIdY();
    int min_pin_index = net.MinBlkPinIdY();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetY();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetY();

    double distance = std::fabs(pin_loc_min - pin_loc_max);
    //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
    //weight = inv_p / (distance + height_epsilon_) * weight_adjust;
    double weight = inv_p / (distance + height_epsilon_);
    if (!is_movable_min && is_movable_max) {
      by[blk_num_max] += (pin_loc_min - offset_max) * weight;
      coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
    } else if (is_movable_min && !is_movable_max) {
      by[blk_num_min] += (pin_loc_max - offset_max) * weight;
      coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
    } else if (is_movable_min && is_movable_max) {
      coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
      coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
      coefficients_y_.emplace_back(blk_num_min, blk_num_max, -weight);
      coefficients_y_.emplace_back(blk_num_max, blk_num_min, -weight);
      double offset_diff = (offset_max - offset_min) * weight;
      by[blk_num_min] += offset_diff;
      by[blk_num_max] -= offset_diff;
    }
  }
  for (int i = 0; i < sz;
       ++i) { // add the diagonal non-zero element for fixed blocks
    if (block_list[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = block_list[i].LLY();
    } else {
      if (block_list[i].LLY() < RegionBottom()
          || block_list[i].URY() > RegionTop()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
}

void GlobalPlacer::BuildProblemStarHPWLX() {
  double wall_time = get_wall_time();
  UpdateMaxMinX();

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();
  std::vector<BlkPairNets> &blk_pair_net_list = blk_pair_net_list_;
  int pair_sz = blk_pair_net_list.size();
#pragma omp parallel for
  for (int i = 0; i < pair_sz; ++i) {
    BlkPairNets &blk_pair = blk_pair_net_list[i];
    blk_pair.ClearX();
    for (auto &edge : blk_pair.edges) {
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
      //bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
      //double offset_max = net.BlockPins()[max_pin_index].OffsetX();

      int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
      double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
      //bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
      //double offset_min = net.BlockPins()[min_pin_index].OffsetX();

      double distance = std::fabs(load_pin_loc - driver_pin_loc);
      //double weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
      double inv_p = net.InvP();
      double weight = inv_p / (distance + width_epsilon_);
      //weight *= weight_adjust;
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
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
#pragma omp parallel for
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) {
      SpMat_diag_x[i].valueRef() = 1;
      bx[i] = block_list[i].LLX();
    } else {
      double diag_val = 0;
      double b = 0;
      for (auto &blk_pair : pair_connect[i]) {
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

void GlobalPlacer::BuildProblemStarHPWLY() {
  double wall_time = get_wall_time();
  UpdateMaxMinY();

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();
  std::vector<BlkPairNets> &blk_pair_net_list = blk_pair_net_list_;
  int pair_sz = blk_pair_net_list.size();
#pragma omp parallel for
  for (int i = 0; i < pair_sz; ++i) {
    BlkPairNets &blk_pair = blk_pair_net_list[i];
    blk_pair.ClearY();
    for (auto &edge : blk_pair.edges) {
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
      //bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
      //double offset_max = net.BlockPins()[max_pin_index].OffsetY();

      int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
      double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
      //bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
      //double offset_min = net.BlockPins()[min_pin_index].OffsetY();

      double distance = std::fabs(load_pin_loc - driver_pin_loc);
      //double weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
      double inv_p = net.InvP();
      double weight = inv_p / (distance + height_epsilon_);
      //weight *= weight_adjust;
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
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
#pragma omp parallel for
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) {
      SpMat_diag_y[i].valueRef() = 1;
      by[i] = block_list[i].LLY();
    } else {
      double diag_val = 0;
      double b = 0;
      for (auto &blk_pair : pair_connect[i]) {
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

double GlobalPlacer::OptimizeQuadraticMetricX(double cg_stop_criterion) {
  double wall_time = get_wall_time();
  if (net_model != 3) {
    Ax.setFromTriplets(coefficients_x_.begin(), coefficients_x_.end());
  }
  wall_time = get_wall_time() - wall_time;
  tot_matrix_from_triplets_x += wall_time;

  int sz = vx.size();
  std::vector<Block> &blocks = ckt_ptr_->Blocks();

  wall_time = get_wall_time();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_x_.compute(Ax); // Ax * vx = bx
  for (int i = 0; i < max_rounds; ++i) {
    vx = cg_x_.solveWithGuess(bx, vx);
    for (int num = 0; num < sz; ++num) {
      blocks[num].SetLLX(vx[num]);
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
  BOOST_LOG_TRIVIAL(trace)
    << "      Metric optimization in X, sequence: " << eval_history << "\n";
  wall_time = get_wall_time() - wall_time;
  tot_cg_solver_time_x += wall_time;

  wall_time = get_wall_time();

  for (int num = 0; num < sz; ++num) {
    blocks[num].SetLLX(vx[num]);
  }
  wall_time = get_wall_time() - wall_time;
  tot_loc_update_time_x += wall_time;

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!"
  );
  return eval_history.back();
}

double GlobalPlacer::OptimizeQuadraticMetricY(double cg_stop_criterion) {
  double wall_time = get_wall_time();
  if (net_model != 3) {
    Ay.setFromTriplets(coefficients_y_.begin(), coefficients_y_.end());
  }
  wall_time = get_wall_time() - wall_time;
  tot_matrix_from_triplets_y += wall_time;

  int sz = vx.size();
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  wall_time = get_wall_time();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_y_.compute(Ay);
  for (int i = 0; i < max_rounds; ++i) {
    vy = cg_y_.solveWithGuess(by, vy);
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

void GlobalPlacer::PullBlockBackToRegion() {
  int sz = vx.size();
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

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

void GlobalPlacer::BuildProblemX() {
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

void GlobalPlacer::BuildProblemY() {
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

double GlobalPlacer::QuadraticPlacement(double net_model_update_stop_criterion) {
  //omp_set_nested(1);
  omp_set_dynamic(0);
  int avail_threads_num = omp_get_max_threads();
  double wall_time = get_wall_time();

#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
  {
    BOOST_LOG_TRIVIAL(trace)
      << "OpenMP threads, " << omp_get_num_threads() << "\n";
    if (omp_get_thread_num() == 0) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch x: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";

      std::vector<Block> &block_list = ckt_ptr_->Blocks();
      for (size_t i = 0; i < block_list.size(); ++i) {
        vx[i] = block_list[i].LLX();
      }

      std::vector<double> eval_history_x;
      int b2b_update_it_x = 0;
      for (b2b_update_it_x = 0; b2b_update_it_x < b2b_update_max_iteration_;
           ++b2b_update_it_x) {
        BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
        BuildProblemX();
        double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
        eval_history_x.push_back(evaluate_result);
        if (eval_history_x.size() >= 3) {
          bool is_converge = IsSeriesConverge(
              eval_history_x,
              3,
              net_model_update_stop_criterion
          );
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
      DaliExpects(
          !eval_history_x.empty(),
          "Cannot return a valid value because the result is not evaluated!"
      );
      lower_bound_hpwlx_.push_back(eval_history_x.back());
    }

    if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch y: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";

      std::vector<Block> &block_list = ckt_ptr_->Blocks();
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
          bool is_converge = IsSeriesConverge(
              eval_history_y, 3,
              net_model_update_stop_criterion
          );
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
      DaliExpects(
          !eval_history_y.empty(),
          "Cannot return a valid value because the result is not evaluated!"
      );
      lower_bound_hpwly_.push_back(eval_history_y.back());
    }
  }

  PullBlockBackToRegion();

  BOOST_LOG_TRIVIAL(info) << "Initial Placement Complete\n";

  wall_time = get_wall_time() - wall_time;
  tot_cg_time += wall_time;
  //omp_set_nested(0);

  if (is_dump_) DumpResult("cg_result_0.txt");

  BackUpBlockLocation();

  return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
}

/****
 * @brief determine the grid bin height and width
 * grid_bin_height and grid_bin_width is determined by the following formula:
 *    grid_bin_height = sqrt(number_of_cell_in_bin_ * average_area / placement_density)
 * the number of bins in the y-direction is given by:
 *    grid_cnt_y = (Top() - Bottom())/grid_bin_height
 *    grid_cnt_x = (Right() - Left())/grid_bin_width
 * And initialize the space of grid_bin_mesh
 */
void GlobalPlacer::InitializeGridBinSize() {
  double grid_bin_area =
      number_of_cell_in_bin_ * ckt_ptr_->AveMovBlkArea() / PlacementDensity();
  grid_bin_height = static_cast<int>(std::round(std::sqrt(grid_bin_area)));
  grid_bin_width = grid_bin_height;
  grid_cnt_x = std::ceil(double(RegionWidth()) / grid_bin_width);
  grid_cnt_y = std::ceil(double(RegionHeight()) / grid_bin_height);
  BOOST_LOG_TRIVIAL(info)
    << "Global placement bin width, height: "
    << grid_bin_width << "  " << grid_bin_height << "\n";

  std::vector<GridBin> temp_grid_bin_column(grid_cnt_y);
  grid_bin_mesh.resize(grid_cnt_x, temp_grid_bin_column);
}

/****
 * @brief set basic attributes for each grid bin.
 * we need to initialize many attributes in every single grid bin, including index,
 * boundaries, area, and potential available white space. The adjacent bin list
 * is cached for the convenience of overfilled bin clustering.
 */
void GlobalPlacer::UpdateAttributesForAllGridBins() {
  for (int i = 0; i < grid_cnt_x; i++) {
    for (int j = 0; j < grid_cnt_y; j++) {
      grid_bin_mesh[i][j].index = {i, j};
      grid_bin_mesh[i][j].bottom = RegionBottom() + j * grid_bin_height;
      grid_bin_mesh[i][j].top = RegionBottom() + (j + 1) * grid_bin_height;
      grid_bin_mesh[i][j].left = RegionLeft() + i * grid_bin_width;
      grid_bin_mesh[i][j].right = RegionLeft() + (i + 1) * grid_bin_width;
      grid_bin_mesh[i][j].white_space = grid_bin_mesh[i][j].Area();
      // at the very beginning, assuming the white space is the same as area
      grid_bin_mesh[i][j].create_adjacent_bin_list(grid_cnt_x, grid_cnt_y);
    }
  }

  // make sure the top placement boundary is the same as the top of the topmost bins
  for (int i = 0; i < grid_cnt_x; ++i) {
    grid_bin_mesh[i][grid_cnt_y - 1].top = RegionTop();
    grid_bin_mesh[i][grid_cnt_y - 1].white_space =
        grid_bin_mesh[i][grid_cnt_y - 1].Area();
  }
  // make sure the right placement boundary is the same as the right of the rightmost bins
  for (int i = 0; i < grid_cnt_y; ++i) {
    grid_bin_mesh[grid_cnt_x - 1][i].right = RegionRight();
    grid_bin_mesh[grid_cnt_x - 1][i].white_space =
        grid_bin_mesh[grid_cnt_x - 1][i].Area();
  }
}

/****
 * @brief find fixed blocks in each grid bin
 * For each fixed block, we need to store its index in grid bins it overlaps with.
 * This can help us to compute available white space in each grid bin.
 */
void GlobalPlacer::UpdateFixedBlocksInGridBins() {
  auto &blocks = ckt_ptr_->Blocks();
  int sz = static_cast<int>(blocks.size());
  for (int i = 0; i < sz; i++) {
    /* find the left, right, bottom, top index of the grid */
    if (blocks[i].IsMovable()) continue;
    bool fixed_blk_out_of_region = int(blocks[i].LLX()) >= RegionRight() ||
        int(blocks[i].URX()) <= RegionLeft() ||
        int(blocks[i].LLY()) >= RegionTop() ||
        int(blocks[i].URY()) <= RegionBottom();
    if (fixed_blk_out_of_region) continue;
    int left_index = (int) std::floor(
        (blocks[i].LLX() - RegionLeft()) / grid_bin_width);
    int right_index = (int) std::floor(
        (blocks[i].URX() - RegionLeft()) / grid_bin_width);
    int bottom_index = (int) std::floor(
        (blocks[i].LLY() - RegionBottom()) / grid_bin_height);
    int top_index = (int) std::floor(
        (blocks[i].URY() - RegionBottom()) / grid_bin_height);
    /* the grid boundaries might be the placement region boundaries
     * if a block touches the rightmost and topmost boundaries,
     * the index need to be fixed to make sure no memory access out of scope */
    if (left_index < 0) left_index = 0;
    if (right_index >= grid_cnt_x) right_index = grid_cnt_x - 1;
    if (bottom_index < 0) bottom_index = 0;
    if (top_index >= grid_cnt_y) top_index = grid_cnt_y - 1;

    /* for each terminal, we will check which grid is inside it, and directly
     * set the all_terminal attribute to true for that grid some small
     * terminals might occupy the same grid, we need to deduct the overlap
     * area from the white space of that grid bin when the final white space
     * is 0, we know this grid bin is occupied by several terminals*/
    for (int j = left_index; j <= right_index; ++j) {
      for (int k = bottom_index; k <= top_index; ++k) {
        /* the following case might happen:
         * the top/right of a fixed block overlap with the bottom/left of
         * a grid box if this case happens, we need to ignore this fixed
         * block for this grid box. */
        bool blk_out_of_bin =
            int(blocks[i].LLX() >= grid_bin_mesh[j][k].right) ||
                int(blocks[i].URX() <= grid_bin_mesh[j][k].left) ||
                int(blocks[i].LLY() >= grid_bin_mesh[j][k].top) ||
                int(blocks[i].URY() <= grid_bin_mesh[j][k].bottom);
        if (blk_out_of_bin) continue;
        grid_bin_mesh[j][k].fixed_blocks.push_back(&(blocks[i]));
      }
    }
  }
}

void GlobalPlacer::UpdateWhiteSpaceInGridBin(GridBin &grid_bin) {
  RectI bin_rect(
      grid_bin.LLX(), grid_bin.LLY(), grid_bin.URX(), grid_bin.URY()
  );

  std::vector<RectI> rects;
  for (auto &fixed_blk_ptr : grid_bin.fixed_blocks) {
    auto &fixed_blk = *fixed_blk_ptr;
    RectI fixed_blk_rect(
        static_cast<int>(std::round(fixed_blk.LLX())),
        static_cast<int>(std::round(fixed_blk.LLY())),
        static_cast<int>(std::round(fixed_blk.URX())),
        static_cast<int>(std::round(fixed_blk.URY()))
    );
    if (bin_rect.IsOverlap(fixed_blk_rect)) {
      rects.push_back(bin_rect.GetOverlapRect(fixed_blk_rect));
    }
  }

  unsigned long long used_area = GetCoverArea(rects);
  if (grid_bin.white_space < used_area) {
    DaliExpects(false,
                "Fixed blocks takes more space than available space? "
                    << grid_bin.white_space << " " << used_area
    );
  }

  grid_bin.white_space -= used_area;
  if (grid_bin.white_space == 0) {
    grid_bin.all_terminal = true;
  }
}

/****
 * This function initialize the grid bin matrix, each bin has an area which
 * can accommodate around number_of_cell_in_bin_ # of cells
 * ****/
void GlobalPlacer::InitGridBins() {
  InitializeGridBinSize();
  UpdateAttributesForAllGridBins();
  UpdateFixedBlocksInGridBins();

  // update white spaces in grid bins
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      UpdateWhiteSpaceInGridBin(grid_bin);
    }
  }
}

/****
* this is a member function to initialize white space look-up table
* this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in this region
* an easier way is to define an accumulate function and store it as a look-up table
* when we want to find the white space in a region, the value can be easily extracted from the look-up table
* ****/
void GlobalPlacer::InitWhiteSpaceLUT() {
  // this for loop is created to initialize the size of the loop-up table
  std::vector<unsigned long long> tmp_vector(grid_cnt_y);
  grid_bin_white_space_LUT.resize(grid_cnt_x, tmp_vector);

  // this for loop is used for computing elements in the look-up table
  // there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
  for (int kx = 0; kx < grid_cnt_x; ++kx) {
    for (int ky = 0; ky < grid_cnt_y; ++ky) {
      grid_bin_white_space_LUT[kx][ky] = 0;
      if (kx == 0) {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_mesh[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx][ky - 1]
                  + grid_bin_mesh[kx][ky].white_space;
        }
      } else {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky]
                  + grid_bin_mesh[kx][ky].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] =
              grid_bin_white_space_LUT[kx - 1][ky]
                  + grid_bin_white_space_LUT[kx][ky - 1]
                  + grid_bin_mesh[kx][ky].white_space
                  - grid_bin_white_space_LUT[kx - 1][ky - 1];
        }
      }
    }
  }
}

void GlobalPlacer::InitializeRoughLegalizer() {
  upper_bound_hpwlx_.clear();
  upper_bound_hpwly_.clear();
  upper_bound_hpwl_.clear();
  InitGridBins();
  InitWhiteSpaceLUT();
}

void GlobalPlacer::LALClose() {
  grid_bin_mesh.clear();
  grid_bin_white_space_LUT.clear();
}

void GlobalPlacer::ClearGridBinFlag() {
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) bin.global_placed = false;
  }
}

unsigned long int GlobalPlacer::LookUpWhiteSpace(GridBinIndex const &ll_index,
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

unsigned long int GlobalPlacer::LookUpWhiteSpace(WindowQuadruple &window) {
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

unsigned long int GlobalPlacer::LookUpBlkArea(WindowQuadruple &window) {
  unsigned long int res = 0;
  for (int x = window.llx; x <= window.urx; ++x) {
    for (int y = window.lly; y <= window.ury; ++y) {
      res += grid_bin_mesh[x][y].cell_area;
    }
  }
  return res;
}

unsigned long int GlobalPlacer::WindowArea(WindowQuadruple &window) {
  unsigned long int res = 0;
  if (window.urx == grid_cnt_x - 1) {
    if (window.ury == grid_cnt_y - 1) {
      res = (window.urx - window.llx) * (window.ury - window.lly)
          * grid_bin_width * grid_bin_height;
      res += (window.urx - window.llx) * grid_bin_width
          * grid_bin_mesh[window.urx][window.ury].Height();
      res += (window.ury - window.lly) * grid_bin_height
          * grid_bin_mesh[window.urx][window.ury].Width();
      res += grid_bin_mesh[window.urx][window.ury].Area();
    } else {
      res = (window.urx - window.llx) * (window.ury - window.lly + 1)
          * grid_bin_width * grid_bin_height;
      res += (window.ury - window.lly + 1) * grid_bin_height
          * grid_bin_mesh[window.urx][window.ury].Width();
    }
  } else {
    if (window.ury == grid_cnt_y - 1) {
      res = (window.urx - window.llx + 1) * (window.ury - window.lly)
          * grid_bin_width * grid_bin_height;
      res += (window.urx - window.llx + 1) * grid_bin_width
          * grid_bin_mesh[window.urx][window.ury].Height();
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

/****
 * this is a member function to update grid bin status, because the cell_list,
 * cell_area and over_fill state can be changed, so we need to update them when necessary
 * ****/
void GlobalPlacer::UpdateGridBinState() {
  double wall_time = get_wall_time();

  // clean the old data
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      grid_bin.cell_list.clear();
      grid_bin.cell_area = 0;
      grid_bin.over_fill = false;
    }
  }

  // for each cell, find the index of the grid bin it should be in.
  // note that in extreme cases, the index might be smaller than 0 or larger
  // than the maximum allowed index, because the cell is on the boundaries,
  // so we need to make some modifications for these extreme cases.
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int sz = static_cast<int>(blocks.size());
  int x_index = 0;
  int y_index = 0;

  for (int i = 0; i < sz; i++) {
    if (blocks[i].IsFixed()) continue;
    x_index = (int) std::floor((blocks[i].X() - RegionLeft()) / grid_bin_width);
    y_index =
        (int) std::floor((blocks[i].Y() - RegionBottom()) / grid_bin_height);
    if (x_index < 0) x_index = 0;
    if (x_index > grid_cnt_x - 1) x_index = grid_cnt_x - 1;
    if (y_index < 0) y_index = 0;
    if (y_index > grid_cnt_y - 1) y_index = grid_cnt_y - 1;
    grid_bin_mesh[x_index][y_index].cell_list.push_back(&(blocks[i]));
    grid_bin_mesh[x_index][y_index].cell_area += blocks[i].Area();
  }

  /**** below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by fixed blocks, but its cell_list is
   *    non-empty, which means there is some cells overlap with this grid bin,
   *    we say it is over_fill
   * 2. if not fully occupied by fixed blocks, but filling_rate is larger than
   *    the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with fixed
   *    blocks in this bin, we also mark it as over_fill
   * ****/
  //TODO: the third criterion might be changed in the next
  bool over_fill = false;
  for (auto &grid_bin_column : grid_bin_mesh) {
    for (auto &grid_bin : grid_bin_column) {
      if (grid_bin.global_placed) {
        grid_bin.over_fill = false;
        continue;
      }
      if (grid_bin.IsAllFixedBlk()) {
        if (!grid_bin.cell_list.empty()) {
          grid_bin.over_fill = true;
        }
      } else {
        grid_bin.filling_rate =
            double(grid_bin.cell_area) / double(grid_bin.white_space);
        if (grid_bin.filling_rate > PlacementDensity()) {
          grid_bin.over_fill = true;
        }
      }
      if (!grid_bin.OverFill()) {
        for (auto &blk_ptr : grid_bin.cell_list) {
          for (auto &fixed_blk_ptr : grid_bin.fixed_blocks) {
            over_fill = blk_ptr->IsOverlap(*fixed_blk_ptr);
            if (over_fill) {
              grid_bin.over_fill = true;
              break;
            }
          }
          if (over_fill) break;
          // two breaks have to be used to break two loops
        }
      }
    }
  }
  update_grid_bin_state_time_ += get_wall_time() - wall_time;
}

void GlobalPlacer::UpdateClusterArea(GridBinCluster &cluster) {
  cluster.total_cell_area = 0;
  cluster.total_white_space = 0;
  for (auto &index : cluster.bin_set) {
    cluster.total_cell_area +=
        grid_bin_mesh[index.x][index.y].cell_area;
    cluster.total_white_space +=
        grid_bin_mesh[index.x][index.y].white_space;
  }
}

void GlobalPlacer::UpdateClusterList() {
  double wall_time = get_wall_time();
  cluster_set.clear();

  int m = (int) grid_bin_mesh.size(); // number of rows
  int n = (int) grid_bin_mesh[0].size(); // number of columns
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j)
      grid_bin_mesh[i][j].cluster_visited = false;
  }
  int cnt = 0;
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      if (grid_bin_mesh[i][j].cluster_visited
          || !grid_bin_mesh[i][j].over_fill)
        continue;
      GridBinIndex b(i, j);
      GridBinCluster H;
      H.bin_set.insert(b);
      grid_bin_mesh[i][j].cluster_visited = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto
              &index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          GridBin &bin = grid_bin_mesh[index.x][index.y];
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
  update_cluster_list_time_ += get_wall_time() - wall_time;
}

void GlobalPlacer::UpdateLargestCluster() {
  if (cluster_set.empty()) return;

  for (auto it = cluster_set.begin(); it != cluster_set.end();) {
    bool is_contact = true;

    // if there is no grid bin has been roughly legalized, then this cluster is the largest one for sure
    for (auto &index : it->bin_set) {
      if (grid_bin_mesh[index.x][index.y].global_placed) {
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
    for (auto &index : it->bin_set) {
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
    for (auto &grid_index : grid_bin_list) {
      int i = grid_index.x;
      int j = grid_index.y;
      if (grid_bin_visited[grid_index]) continue; // if this grid bin has been visited continue
      if (grid_bin_mesh[i][j].global_placed) continue; // if this grid bin has been roughly legalized
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
              &index : grid_bin_mesh[b.x][b.y].adjacent_bin_index) {
          if (grid_bin_visited.find(index)
              == grid_bin_visited.end())
            continue; // this index is not in the cluster
          if (grid_bin_visited[index]) continue; // this index has been visited
          if (grid_bin_mesh[index.x][index.y].global_placed) continue; // if this grid bin has been roughly legalized
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

void GlobalPlacer::FindMinimumBoxForLargestCluster() {
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
  BoxBin R;
  R.cut_direction_x = false;

  R.ll_index.x = grid_cnt_x - 1;
  R.ll_index.y = grid_cnt_y - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  auto it = cluster_set.begin();
  for (auto &index : it->bin_set) {
    R.ll_index.x = std::min(R.ll_index.x, index.x);
    R.ur_index.x = std::max(R.ur_index.x, index.x);
    R.ll_index.y = std::min(R.ll_index.y, index.y);
    R.ur_index.y = std::max(R.ur_index.y, index.y);
  }
  while (true) {
    // update cell area, white space, and thus filling rate to determine whether to expand this box or not
    R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
    R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                          grid_bin_mesh);
    if (R.filling_rate > PlacementDensity()) {
      R.ExpandBox(grid_cnt_x, grid_cnt_y);
    } else {
      break;
    }
    //BOOST_LOG_TRIVIAL(info)   << R.total_white_space << "  " << R.filling_rate << "  " << FillingRate() << "\n";
  }

  R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
  R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT,
                                        grid_bin_mesh);
  R.UpdateCellList(grid_bin_mesh);
  R.ll_point.x = grid_bin_mesh[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_mesh[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_mesh[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_mesh[R.ur_index.x][R.ur_index.y].top;

  R.left = int(R.ll_point.x);
  R.bottom = int(R.ll_point.y);
  R.right = int(R.ur_point.x);
  R.top = int(R.ur_point.y);

  if (R.ll_index == R.ur_index) {
    R.UpdateFixedBlkList(grid_bin_mesh);
    if (R.IsContainFixedBlk()) {
      R.UpdateObsBoundary();
    }
  }
  queue_box_bin.push(R);
  //BOOST_LOG_TRIVIAL(info)   << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
  //BOOST_LOG_TRIVIAL(info)   << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

  for (int kx = R.ll_index.x; kx <= R.ur_index.x; ++kx) {
    for (int ky = R.ll_index.y; ky <= R.ur_index.y; ++ky) {
      grid_bin_mesh[kx][ky].global_placed = true;
    }
  }

  find_minimum_box_for_largest_cluster_time_ += get_wall_time() - wall_time;
}

void GlobalPlacer::SplitBox(BoxBin &box) {
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
  box1.update_cell_area_white_space(grid_bin_mesh);
  box2.update_cell_area_white_space(grid_bin_mesh);
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

  box1.update_boundaries(grid_bin_mesh);
  box2.update_boundaries(grid_bin_mesh);

  if (dominating_box_flag == 0) {
    //BOOST_LOG_TRIVIAL(info)   << "cell list size: " << box.cell_list.size() << "\n";
    //box.update_cell_area(block_list);
    //BOOST_LOG_TRIVIAL(info)   << "total_cell_area: " << box.total_cell_area << "\n";
    box.update_cut_point_cell_list_low_high(
        box1.total_white_space,
        box2.total_white_space
    );
    box1.cell_list = box.cell_list_low;
    box2.cell_list = box.cell_list_high;
    box1.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box1.ur_point = box.cut_ur_point;
    box2.ll_point = box.cut_ll_point;
    box1.total_cell_area = box.total_cell_area_low;
    box2.total_cell_area = box.total_cell_area_high;

    if (box1.ll_index == box1.ur_index) {
      box1.UpdateFixedBlkList(grid_bin_mesh);
      box1.UpdateObsBoundary();
    }
    if (box2.ll_index == box2.ur_index) {
      box2.UpdateFixedBlkList(grid_bin_mesh);
      box2.UpdateObsBoundary();
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
      box2.UpdateFixedBlkList(grid_bin_mesh);
      box2.UpdateObsBoundary();
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
      box1.UpdateFixedBlkList(grid_bin_mesh);
      box1.UpdateObsBoundary();
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

/****
 *
 *
 * @param box: the BoxBin which needs to be further splitted into two sub-boxes
 */
void GlobalPlacer::SplitGridBox(BoxBin &box) {
  // 1. create two sub-boxes
  BoxBin box1, box2;
  // the first sub-box should have the same lower left corner as the original box
  box1.left = box.left;
  box1.bottom = box.bottom;
  box1.ll_index = box.ll_index;
  box1.ur_index = box.ur_index;
  // the second sub-box should have the same upper right corner as the original box
  box2.right = box.right;
  box2.top = box.top;
  box2.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;

  // 2. split along the direction with more boundary lines
  if (box.IsMoreHorizontalCutlines()) {
    box.cut_direction_x = true;
    // split the original box along the first horizontal cur line
    box1.right = box.right;
    box1.top = box.horizontal_cutlines[0];
    box2.left = box.left;
    box2.bottom = box.horizontal_cutlines[0];
    box1.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);
    box2.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);

    if (
        double(box1.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary();
      queue_box_bin.push(box2);
    } else if (
        double(box2.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary();
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(
          box1.total_white_space,
          box2.total_white_space
      );
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  } else {
    //box.Report();
    box.cut_direction_x = false;
    box1.right = box.vertical_cutlines[0];
    box1.top = box.top;
    box2.left = box.vertical_cutlines[0];
    box2.bottom = box.bottom;
    box1.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);
    box2.UpdateWhiteSpaceAndFixedblocks(box.fixed_blocks);

    if (
        double(box1.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary();
      queue_box_bin.push(box2);
    } else if (
        double(box2.total_white_space) / (double) box.total_white_space <= 0.01
        ) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary();
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(
          box1.total_white_space,
          box2.total_white_space
      );
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary();
      box2.UpdateObsBoundary();
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  }
}

void GlobalPlacer::PlaceBlkInBox(BoxBin &box) {
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

  int sz = static_cast<int>(box.cell_list.size());
  std::vector<std::pair<Block *, double>> index_loc_list_x(sz);
  std::vector<std::pair<Block *, double>> index_loc_list_y(sz);
  GridBin &grid_bin = grid_bin_mesh[box.ll_index.x][box.ll_index.y];
  for (int i = 0; i < sz; ++i) {
    Block *blk_ptr = box.cell_list[i];
    index_loc_list_x[i].first = blk_ptr;
    index_loc_list_x[i].second = blk_ptr->X();
    index_loc_list_y[i].first = blk_ptr;
    index_loc_list_y[i].second = blk_ptr->Y();
    grid_bin.cell_list.push_back(blk_ptr);
    grid_bin.cell_area += blk_ptr->Area();
  }

  std::sort(
      index_loc_list_x.begin(),
      index_loc_list_x.end(),
      [](const std::pair<Block *, double> &p1,
         const std::pair<Block *, double> &p2) {
        return p1.second < p2.second;
      }
  );
  double total_length = 0;
  for (auto &blk_ptr : box.cell_list) {
    total_length += blk_ptr->Width();
  }
  double cur_pos = 0;
  int box_width = box.right - box.left;
  for (auto &pair : index_loc_list_x) {
    Block *blk_ptr = pair.first;
    double center_x = box.left + cur_pos / total_length * box_width;
    blk_ptr->SetCenterX(center_x);
    cur_pos += blk_ptr->Width();
    if (std::isnan(center_x)) {
      std::cout << "x " << total_length << "\n";
      box.Report();
      std::cout << std::endl;
      exit(1);
    }
  }

  std::sort(
      index_loc_list_y.begin(),
      index_loc_list_y.end(),
      [](const std::pair<Block *, double> &p1,
         const std::pair<Block *, double> &p2) {
        return p1.second < p2.second;
      }
  );
  total_length = 0;
  for (auto &blk_ptr : box.cell_list) {
    total_length += blk_ptr->Height();
  }
  cur_pos = 0;
  int box_height = box.top - box.bottom;
  for (auto &pair : index_loc_list_y) {
    Block *blk_ptr = pair.first;
    double center_y = box.bottom + cur_pos / total_length * box_height;
    if (std::isnan(center_y)) {
      std::cout << "y " << total_length << "\n";
      box.Report();
      std::cout << std::endl;
      exit(1);
    }
    blk_ptr->SetCenterY(center_y);
    cur_pos += blk_ptr->Height();
  }
}

void GlobalPlacer::RoughLegalBlkInBox(BoxBin &box) {
  int sz = box.cell_list.size();
  if (sz == 0) return;
  std::vector<int> row_start;
  row_start.assign(box.top - box.bottom + 1, box.left);

  std::vector<BlkInitPair> index_loc_list;
  BlkInitPair tmp_index_loc_pair(nullptr, 0, 0);
  index_loc_list.resize(sz, tmp_index_loc_pair);

  for (int i = 0; i < sz; ++i) {
    Block *blk_ptr = box.cell_list[i];
    index_loc_list[i].blk_ptr = blk_ptr;
    index_loc_list[i].x = blk_ptr->LLX();
    index_loc_list[i].y = blk_ptr->LLY();
  }
  std::sort(
      index_loc_list.begin(), index_loc_list.end(),
      [](const BlkInitPair &pair0, const BlkInitPair &pair1) {
        return (pair0.x < pair1.x)
            || ((pair0.x == pair1.x) && (pair0.y < pair1.y));
      }
  );

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

  for (auto &pair : index_loc_list) {
    auto &block = *(pair.blk_ptr);

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
  for (auto &pair : index_loc_list) {
    auto &block = *(pair.blk_ptr);
    block.SetCenterX((block.X() - min_x) / span_x * box_width + box.left);
  }

}

double GlobalPlacer::BlkOverlapArea(Block *node1, Block *node2) {
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

void GlobalPlacer::PlaceBlkInBoxBisection(BoxBin &box) {
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

      int ave_blk_height = std::ceil(ckt_ptr_->AveMovBlkHeight());
      //BOOST_LOG_TRIVIAL(info)   << "Average block height: " << ave_blk_height << "  " << GetCircuitRef().AveMovBlkHeight() << "\n";
      front_box.cut_direction_x =
          (front_box.top - front_box.bottom > ave_blk_height);
      int cut_line_w = 0; // cut-line for White space
      front_box.update_cut_point_cell_list_low_high_leaf(
          cut_line_w,
          ave_blk_height
      );
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
        for (auto &blk_ptr : front_box.cell_list) {
          blk_ptr->SetCenterX((front_box.left + front_box.right) / 2.0);
          blk_ptr->SetCenterY((front_box.bottom + front_box.top) / 2.0);
        }
      } else {
        PlaceBlkInBox(front_box);
        //RoughLegalBlkInBox(front_box);
      }
    }
    box_Q.pop();
  }
}

void GlobalPlacer::UpdateGridBinBlocks(BoxBin &box) {
  /****
 * Update the block list, area, and overfilled state of a box bin if this box is also a grid bin box
 * ****/

  // if this box is not a grid bin, then do nothing
  if (box.ll_index == box.ur_index) {
    int x_index = box.ll_index.x;
    int y_index = box.ll_index.y;
    GridBin &grid_bin = grid_bin_mesh[x_index][y_index];
    if (grid_bin.left == box.left &&
        grid_bin.bottom == box.bottom &&
        grid_bin.right == box.right &&
        grid_bin.top == box.top) {
      grid_bin.cell_list.clear();
      grid_bin.cell_area = 0;
      grid_bin.over_fill = false;

      for (auto &blk_ptr : box.cell_list) {
        grid_bin.cell_list.push_back(blk_ptr);
        grid_bin.cell_area += blk_ptr->Area();
      }
    }
  }
}

/****
 * keep splitting the biggest box to many small boxes, and keep update the shape
 * of each box and cells should be assigned to the box
 * @return true if succeed, false if fail
 */
bool GlobalPlacer::RecursiveBisectionblockspreading() {
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

  recursive_bisection_block_spreading_time_ += get_wall_time() - wall_time;
  return true;
}

void GlobalPlacer::BackUpBlockLocation() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
  for (int i = 0; i < sz; ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

void GlobalPlacer::UpdateAnchorLocation() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  for (int i = 0; i < sz; ++i) {
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

void GlobalPlacer::UpdateAnchorNetWeight() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  // X direction
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    double pin_loc0 = block_list[i].LLX();
    double pin_loc1 = x_anchor[i];
    double displacement = std::fabs(pin_loc0 - pin_loc1);
    double weight = alpha / (displacement + width_epsilon_);
    x_anchor_weight[i] = weight;
  }

  // Y direction
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    double pin_loc0 = block_list[i].LLY();
    double pin_loc1 = y_anchor[i];
    double displacement = std::fabs(pin_loc0 - pin_loc1);
    double weight = alpha / (displacement + width_epsilon_);
    y_anchor_weight[i] = weight;
  }
}

void GlobalPlacer::BuildProblemWithAnchorX() {
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

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLX();
    pin_loc1 = x_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    bx[i] += pin_loc1 * weight;
    if (net_model == 3) {
      SpMat_diag_x[i].valueRef() += weight;
    } else {
      coefficients_x_.emplace_back(T(i, i, weight));
    }
  }
  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}

void GlobalPlacer::BuildProblemWithAnchorY() {
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

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLY();
    pin_loc1 = y_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    by[i] += pin_loc1 * weight;
    if (net_model == 3) {
      SpMat_diag_y[i].valueRef() += weight;
    } else {
      coefficients_y_.emplace_back(T(i, i, weight));
    }
  }
  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
}

double GlobalPlacer::QuadraticPlacementWithAnchor(double net_model_update_stop_criterion) {
  //omp_set_nested(1);
  omp_set_dynamic(0);
  int avail_threads_num = omp_get_max_threads();
  //BOOST_LOG_TRIVIAL(info)   << "total threads: " << avail_threads_num << "\n";
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  UpdateAnchorLocation();
  UpdateAnchorAlpha();
  //UpdateAnchorNetWeight();
  BOOST_LOG_TRIVIAL(trace) << "alpha: " << alpha << "\n";

#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
  {
    BOOST_LOG_TRIVIAL(trace)
      << "OpenMP threads, " << omp_get_num_threads() << "\n";
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
        double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
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
                  "Cannot return a valid value because the result is not evaluated!");
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
                  "Cannot return a valid value because the result is not evaluated!");
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

  if (is_dump_) DumpResult("cg_result_" + std::to_string(cur_iter_) + ".txt");
  BackUpBlockLocation();
  return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
}

void GlobalPlacer::UpdateAnchorAlpha() {
  if (net_model == 0) {
    if (0 <= cur_iter_ && cur_iter_ < 5) {
      alpha_step = 0.005;
    } else if (cur_iter_ < 10) {
      alpha_step = 0.01;
    } else if (cur_iter_ < 15) {
      alpha_step = 0.02;
    } else {
      alpha_step = 0.03;
    }
    alpha += alpha_step;
  } else if (net_model == 1) {
    alpha = 0.002 * cur_iter_;
  } else if (net_model == 2) {
    alpha = 0.005 * cur_iter_;
  } else {
    alpha = 0.002 * cur_iter_;
  }
}

double GlobalPlacer::LookAheadLegalization() {
  double cpu_time = get_cpu_time();

  ClearGridBinFlag();
  UpdateGridBinState();
  UpdateClusterList();
  do {
    UpdateLargestCluster();
    FindMinimumBoxForLargestCluster();
    RecursiveBisectionblockspreading();
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

  if (is_dump_) {
    DumpResult("lal_result_" + std::to_string(cur_iter_) + ".txt");
    DumpLookAheadDisplacement("displace_" + std::to_string(cur_iter_), 1);
  }

  BOOST_LOG_TRIVIAL(debug)
    << "(UpdateGridBinState time: " << update_grid_bin_state_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(UpdateClusterList time: " << update_cluster_list_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(FindMinimumBoxForLargestCluster time: "
    << find_minimum_box_for_largest_cluster_time_ << "s)\n";
  BOOST_LOG_TRIVIAL(debug)
    << "(RecursiveBisectionblockspreading time: "
    << recursive_bisection_block_spreading_time_ << "s)\n";

  return evaluate_result_x + evaluate_result_y;
}

/****
* Returns true of false indicating the convergence of the global placement.
* Stopping criteria (SimPL, option 1):
*    (a). the gap is reduced to 25% of the gap in the tenth iteration and upper-bound solution stops improving
*    (b). the gap is smaller than 10% of the gap in the tenth iteration
* Stopping criteria (POLAR, option 2):
*    the gap between lower bound wirelength and upper bound wirelength is less than 8%
* ****/
bool GlobalPlacer::IsPlacementConverge() {
  bool res = false;
  if (convergence_criteria_ == 1) {
    // (a) and (b) requires at least 10 iterations
    if (lower_bound_hpwl_.size() <= 10) {
      res = false;
    } else {
      double tenth_gap = upper_bound_hpwl_[9] - lower_bound_hpwl_[9];
      double last_gap = upper_bound_hpwl_.back() - lower_bound_hpwl_.back();
      double gap_ratio = last_gap / tenth_gap;
      if (gap_ratio < 0.1) { // (a)
        res = true;
      } else if (gap_ratio < 0.25) { // (b)
        res = IsSeriesConverge(
            upper_bound_hpwl_,
            3,
            simpl_LAL_converge_criterion_
        );
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

void GlobalPlacer::CheckOptimizerAndLegalizer() {
  if (optimizer_ == nullptr) {
    optimizer_ = new B2BHpwlOptimizer(ckt_ptr_);
  }
  if (legalizer_ == nullptr) {
    legalizer_ = new LookAheadLegalizer(ckt_ptr_);
  }
}

#if true
bool GlobalPlacer::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n";
  BOOST_LOG_TRIVIAL(info) << "Start global placement\n";

  SanityCheck();
  InitializeHpwlOptimizer();
  InitializeRoughLegalizer();
  //InitializeBlockLocationNormal();
  InitializeBlockLocationUniform();
  if (net_model == 3) {
    InitializeDriverLoadPairs();
  }

  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info) << "Net list empty? Skip global placement!\n";
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
  BOOST_LOG_TRIVIAL(info)
    << "It " << cur_iter_ << ": \t"
    << std::scientific << std::setprecision(4)
    << lower_bound_hpwl_.back() << " "
    << upper_bound_hpwl_.back() << "\n";

  for (cur_iter_ = 1; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(debug)
      << "----------------------------------------------\n";
    BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";

    eval_res = QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);

    eval_res = LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);

    BOOST_LOG_TRIVIAL(info)
      << "It " << cur_iter_ << ": \t"
      << std::scientific << std::setprecision(4)
      << lower_bound_hpwl_.back() << " "
      << upper_bound_hpwl_.back() << "\n";

    if (IsPlacementConverge()) { // if HPWL converges
      BOOST_LOG_TRIVIAL(info)
        << "Iterative look-ahead legalization complete\n";
      BOOST_LOG_TRIVIAL(info)
        << "Total number of iteration: "
        << cur_iter_ + 1 << "\n";
      break;
    }
  }
  BOOST_LOG_TRIVIAL(info)
    << "Lower bound: " << lower_bound_hpwl_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "Upper bound: " << upper_bound_hpwl_ << "\n";

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m Global Placement complete\033[0m\n";
  BOOST_LOG_TRIVIAL(info)
    << "(cg time: " << tot_cg_time << "s, lal time: " << tot_lal_time << "s)\n";
  BOOST_LOG_TRIVIAL(info)
    << "total triplets time: "
    << tot_triplets_time_x << "s, "
    << tot_triplets_time_y << "s, "
    << tot_triplets_time_x + tot_triplets_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total matrix from triplets time: "
    << tot_matrix_from_triplets_x << "s, "
    << tot_matrix_from_triplets_y << "s, "
    << tot_matrix_from_triplets_x + tot_matrix_from_triplets_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total cg solver time: "
    << tot_cg_solver_time_x << "s, "
    << tot_cg_solver_time_y << "s, "
    << tot_cg_solver_time_x + tot_cg_solver_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total loc update time: "
    << tot_loc_update_time_x << "s, "
    << tot_loc_update_time_y << "s, "
    << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x
      + tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y
      + tot_cg_solver_time_y + tot_loc_update_time_y;
  BOOST_LOG_TRIVIAL(info)
    << "total x/y time: "
    << tot_time_x << "s, "
    << tot_time_y << "s, "
    << tot_time_x + tot_time_y << "s\n";
  LALClose();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";
  ReportMemory();

  return true;
}
#else
bool GlobalPlacer::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  if (ckt_ptr_->Blocks().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Block list empty? Skip global placement!\n"
      << "\033[0;36m Global Placement complete\033[0m\n";
    return true;
  }
  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Net list empty? Skip global placement!\n"
      << "\033[0;36m Global Placement complete\033[0m\n";
    return true;
  }

  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start global placement\n";

  SanityCheck();
  CheckOptimizerAndLegalizer();
  optimizer_->Initialize();
  legalizer_->Initialize(PlacementDensity());
  //InitializeBlockLocationNormal();
  InitializeBlockLocationUniform();
  if (net_model == 3) {
    InitializeDriverLoadPairs();
  }

  // initial placement
  BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";
  double eval_res = optimizer_->QuadraticPlacement(net_model_update_stop_criterion_);
  lower_bound_hpwl_.push_back(eval_res);
  eval_res = legalizer_->LookAheadLegalization();
  upper_bound_hpwl_.push_back(eval_res);
  BOOST_LOG_TRIVIAL(info)
    << "It " << cur_iter_ << ": \t"
    << std::scientific << std::setprecision(4)
    << lower_bound_hpwl_.back() << " "
    << upper_bound_hpwl_.back() << "\n";

  for (cur_iter_ = 1; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(debug)
      << "----------------------------------------------\n";
    BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";

    eval_res = QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);

    eval_res = LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);

    BOOST_LOG_TRIVIAL(info)
      << "It " << cur_iter_ << ": \t"
      << std::scientific << std::setprecision(4)
      << lower_bound_hpwl_.back() << " "
      << upper_bound_hpwl_.back() << "\n";

    if (IsPlacementConverge()) { // if HPWL converges
      BOOST_LOG_TRIVIAL(info)
        << "Iterative look-ahead legalization complete\n";
      BOOST_LOG_TRIVIAL(info)
        << "Total number of iteration: "
        << cur_iter_ + 1 << "\n";
      break;
    }
  }
  BOOST_LOG_TRIVIAL(info)
    << "Lower bound: " << lower_bound_hpwl_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "Upper bound: " << upper_bound_hpwl_ << "\n";

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m Global Placement complete\033[0m\n";
  BOOST_LOG_TRIVIAL(info)
    << "(cg time: " << tot_cg_time << "s, lal time: " << tot_lal_time << "s)\n";
  BOOST_LOG_TRIVIAL(info)
    << "total triplets time: "
    << tot_triplets_time_x << "s, "
    << tot_triplets_time_y << "s, "
    << tot_triplets_time_x + tot_triplets_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total matrix from triplets time: "
    << tot_matrix_from_triplets_x << "s, "
    << tot_matrix_from_triplets_y << "s, "
    << tot_matrix_from_triplets_x + tot_matrix_from_triplets_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total cg solver time: "
    << tot_cg_solver_time_x << "s, "
    << tot_cg_solver_time_y << "s, "
    << tot_cg_solver_time_x + tot_cg_solver_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total loc update time: "
    << tot_loc_update_time_x << "s, "
    << tot_loc_update_time_y << "s, "
    << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x
      + tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y
      + tot_cg_solver_time_y + tot_loc_update_time_y;
  BOOST_LOG_TRIVIAL(info)
    << "total x/y time: "
    << tot_time_x << "s, "
    << tot_time_y << "s, "
    << tot_time_x + tot_time_y << "s\n";
  LALClose();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";
  ReportMemory();

  return true;
}
#endif

void GlobalPlacer::SetDump(bool s_dump) {
  is_dump_ = s_dump;
}

void GlobalPlacer::DumpResult(std::string const &name_of_file) {
  //UpdateGridBinState();
  static int counter = 0;
  //BOOST_LOG_TRIVIAL(info)   << "DumpNum:" << counter << "\n";
  ckt_ptr_->GenMATLABTable(name_of_file);
  //write_not_all_terminal_grid_bins("grid_bin_not_all_terminal" + std::to_string(counter) + ".txt");
  //write_overfill_grid_bins("grid_bin_overfill" + std::to_string(counter) + ".txt");
  ++counter;
}

/****
* Dump the displacement of all cells during look ahead legalization.
* @param mode:
*    if 0: dump displacement for MATLAB visualization (quiver/vector plot)
*    if 1: dump displacement for distribution analysis, unit in average cell height
*    if 2: dump both
* ****/
void GlobalPlacer::DumpLookAheadDisplacement(
    std::string const &base_name,
    int mode
) {
  if (mode < 0 || mode > 2) return;
  if (mode == 0 || mode == 2) {
    std::string name_of_file = base_name + "quiver.txt";
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    DaliExpects(ckt_ptr_ != nullptr,
                "Set input circuit before starting anything");
    std::vector<Block> &block_list = ckt_ptr_->Blocks();
    int sz = ckt_ptr_->design().RealBlkCnt();
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

    DaliExpects(ckt_ptr_ != nullptr,
                "Set input circuit before starting anything");
    double ave_height = ckt_ptr_->AveMovBlkHeight();
    std::vector<Block> &block_list = ckt_ptr_->Blocks();
    int sz = ckt_ptr_->design().RealBlkCnt();
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

void GlobalPlacer::DrawBlockNetList(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open input file " + name_of_file);
  ost << RegionLeft() << " " << RegionBottom() << " "
      << RegionRight() - RegionLeft() << " "
      << RegionTop() - RegionBottom() << "\n";
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  for (auto &block : block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " "
        << block.Height() << "\n";
  }
  ost.close();
}

void GlobalPlacer::write_all_terminal_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is true */
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) {
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

void GlobalPlacer::write_not_all_terminal_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is false */
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) {
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

void GlobalPlacer::write_overfill_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is true */
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) {
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

void GlobalPlacer::write_not_overfill_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is false */
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column : grid_bin_mesh) {
    for (auto &bin : bin_column) {
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

void GlobalPlacer::write_first_n_bin_cluster(std::string const &name_of_file,
                                             size_t n) {
  /* this is a member function for testing, print the first n over_filled clusters */
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open file" + name_of_file);
  auto it = cluster_set.begin();
  for (size_t i = 0; i < n; ++i, --it) {
    for (auto &index : it->bin_set) {
      double low_x, low_y, width, height;
      GridBin *GridBin = &grid_bin_mesh[index.x][index.y];
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

void GlobalPlacer::write_first_bin_cluster(std::string const &name_of_file) {
  /* this is a member function for testing, print the first one over_filled clusters */
  write_first_n_bin_cluster(name_of_file, 1);
}

void GlobalPlacer::write_all_bin_cluster(const std::string &name_of_file) {
  /* this is a member function for testing, print all over_filled clusters */
  write_first_n_bin_cluster(name_of_file, cluster_set.size());
}

void GlobalPlacer::write_first_box(std::string const &name_of_file) {
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

void GlobalPlacer::write_first_box_cell_bounding(std::string const &name_of_file) {
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

bool GlobalPlacer::IsSeriesConverge(
    std::vector<double> &data,
    int window_size,
    double tolerance
) {
  int sz = (int) data.size();
  if (sz < window_size) {
    return false;
  }
  double max_val = -DBL_MAX;
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

bool GlobalPlacer::IsSeriesOscillate(std::vector<double> &data, int length) {
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
/****
 * @brief This function is a wrapper to report HPWL before and after block
 * location initialization using different methods.
 *
 * @param mode: the method to initialize the block locations
 * @param std_dev: the standard deviation if normal distribution is used
 */
void GlobalPlacer::BlockLocationInitialization_(int mode, double std_dev) {
  BOOST_LOG_TRIVIAL(info)
    << "HPWL before random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";

  if (mode == 0) {
    BlockLocationUniformInitialization_();
  } else if (mode == 1) {
    BlockLocationNormalInitialization_(std_dev);
  }

  BOOST_LOG_TRIVIAL(info)
    << "HPWL after random initialization: " << ckt_ptr_->WeightedHPWL() << "\n";

  if (is_dump_) DumpResult("rand_init.txt");
}

/****
 * @brief Initialize the location of blocks uniformly across the placement region.
 */
void GlobalPlacer::BlockLocationUniformInitialization_() {
  // initialize the random number generator
  std::minstd_rand0 generator{1};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = RegionWidth();
  int region_height = RegionHeight();
  for (auto &blk : blocks) {
    if (!blk.IsMovable()) continue;
    double init_x = RegionLeft() + region_width * distribution(generator);
    double init_y = RegionBottom() + region_height * distribution(generator);
    blk.SetCenterX(init_x);
    blk.SetCenterY(init_y);
  }
  BOOST_LOG_TRIVIAL(info)
    << "Block location uniform initialization complete\n";
}

/****
 * @brief Initialize the location of blocks using the normal distribution.
 *
 * @param std_dev: the deviation of cells around the center of the placement region
 */
void GlobalPlacer::BlockLocationNormalInitialization_(double std_dev) {
  // initialize the random number generator
  std::minstd_rand0 generator{1};
  std::normal_distribution<double> normal_distribution(0.0, std_dev);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = RegionWidth();
  int region_height = RegionHeight();
  double region_center_x = (RegionRight() + RegionLeft()) / 2.0;
  double region_center_y = (RegionTop() + RegionBottom()) / 2.0;
  for (auto &block : blocks) {
    if (!block.IsMovable()) continue;
    double x = region_center_x + region_width * normal_distribution(generator);
    double y = region_center_y + region_height * normal_distribution(generator);
    x = std::max(x, (double) RegionLeft());
    x = std::min(x, (double) RegionRight());
    y = std::max(y, (double) RegionBottom());
    y = std::min(y, (double) RegionTop());
    block.SetCenterX(x);
    block.SetCenterY(y);
  }
  BOOST_LOG_TRIVIAL(info)
    << "Block location gaussian initialization complete\n";
}

}
