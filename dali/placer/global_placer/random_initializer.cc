/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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

#include "random_initializer.h"

#include <algorithm>
#include <random>

#include "dali/common/logging.h"

namespace dali {

RandomInitializer::RandomInitializer(
    Circuit *ckt_ptr,
    unsigned int random_seed
) : ckt_ptr_(ckt_ptr),
    random_seed_(random_seed) {
  DaliExpects(ckt_ptr_ != nullptr, "Ckt is a null ptr?");
  initializer_name_ = "abstract random";
}

void RandomInitializer::SetShouldSaveIntermediateResult(
    bool should_save_intermediate_result
) {
  should_save_intermediate_result_ = should_save_intermediate_result;
}

void RandomInitializer::PrintStartStatement() {
  elapsed_time_.RecordStartTime();
  BOOST_LOG_TRIVIAL(info)
    << "  Block location initialization:\n"
    << "    HPWL before, " << ckt_ptr_->WeightedHPWL() << "\n";
}

void RandomInitializer::SetParameters(
    [[maybe_unused]]std::unordered_map<std::string, std::string> &params_dict
) {}

void RandomInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(debug)
    << "    " << initializer_name_ << " initialization complete\n";
  BOOST_LOG_TRIVIAL(info)
    << "    HPWL after, " << ckt_ptr_->WeightedHPWL() << "\n";
  elapsed_time_.RecordEndTime();
  elapsed_time_.PrintTimeElapsed(boost::log::trivial::debug);
  if (should_save_intermediate_result_) {
    ckt_ptr_->GenMATLABTable("rand_init.txt");
  }
}

UniformInitializer::UniformInitializer(
    Circuit *ckt_ptr,
    unsigned int random_seed
) : RandomInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "uniform";
}

void UniformInitializer::RandomPlace() {
  PrintStartStatement();

  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();

  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  for (auto &&blk : blocks) {
    if (!blk.IsMovable()) continue;
    double init_x = region_llx + region_width * distribution(generator);
    double init_y = region_lly + region_height * distribution(generator);
    blk.SetCenterX(init_x);
    blk.SetCenterY(init_y);
  }

  PrintEndStatement();
}

GaussianInitializer::GaussianInitializer(
    Circuit *ckt_ptr,
    unsigned int random_seed
) : RandomInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "Gaussian";
}

void GaussianInitializer::SetParameters(
    std::unordered_map<std::string, std::string> &params_dict
) {
  std::string std_dev_name = "std_dev";
  if (params_dict.find(std_dev_name) != params_dict.end()) {
    try {
      std_dev_ = std::stod(params_dict.at(std_dev_name));
    } catch (...) {
      DaliFatal("Failed to convert "
                    << params_dict.at(std_dev_name) << " to a double");
    }
  }
}

void GaussianInitializer::RandomPlace() {
  PrintStartStatement();
  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::normal_distribution<double> normal_distribution(0.0, std_dev_);

  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_llx = ckt_ptr_->RegionLLX();
  int region_urx = ckt_ptr_->RegionURX();
  int region_lly = ckt_ptr_->RegionLLY();
  int region_ury = ckt_ptr_->RegionURY();
  double center_x = (region_urx + region_llx) / 2.0;
  double center_y = (region_ury + region_lly) / 2.0;
  for (auto &&blk : ckt_ptr_->Blocks()) {
    if (!blk.IsMovable()) continue;
    double x = center_x + region_width * normal_distribution(generator);
    double y = center_y + region_height * normal_distribution(generator);
    x = std::max(x, (double) region_llx);
    x = std::min(x, (double) region_urx);
    y = std::max(y, (double) region_lly);
    y = std::min(y, (double) region_ury);
    blk.SetCenterX(x);
    blk.SetCenterY(y);
  }

  PrintEndStatement();
}

std::vector<Block *> &InitializerGridBin::Macros() {
  return macros_;
}

double InitializerGridBin::GetDensity() const {
  return density_;
}

void InitializerGridBin::UpdateDensity() {
  density_ = used_area_ / total_area_;
}

void InitializerGridBin::SetBoundary(
    double lx,
    double ly,
    double ux,
    double uy
) {
  lx_ = lx;
  ly_ = ly;
  ux_ = ux;
  uy_ = uy;
}

void InitializerGridBin::UpdateTotalArea() {
  total_area_ = (ux_ - lx_) * (uy_ - ly_);
}

void InitializerGridBin::UpdateMacroArea() {
  used_area_ = 0;
  for (auto &&
        macro_ptr : macros_) { // TODO: use GetCoverArea() to get the total macro area
    used_area_ += macro_ptr->Area(); // TODO: use long long instead of double
  }
  UpdateDensity();
}

void InitializerGridBin::AddBlock(Block *blk) {
  blocks_.emplace_back(blk);
  used_area_ += blk->Area();
  UpdateDensity();
}

MonteCarloInitializer::MonteCarloInitializer(
    Circuit *ckt_ptr,
    unsigned int random_seed
) : RandomInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "Monte Carlo";
}

void MonteCarloInitializer::RandomPlace() {
  PrintStartStatement();

  InitializeGridBin();
  AssignFixedMacroToGridBin();

  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();

  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  for (auto &&blk : blocks) {
    if (!blk.IsMovable()) continue;
    for (int i = 0; i < num_trials_; ++i) {
      double init_x = region_llx + region_width * distribution(generator);
      double init_y = region_lly + region_height * distribution(generator);
      blk.SetCenterX(init_x);
      blk.SetCenterY(init_y);
      if (IsBlkLocationValid(blk)) break;
    }
  }

  PrintEndStatement();
}

/****
 * @brief Initialize a grid bin to store fixed macros in each bin
 */
void MonteCarloInitializer::InitializeGridBin() {
  auto region_width = static_cast<double>(ckt_ptr_->RegionWidth());
  double average_blk_width = ckt_ptr_->AveMovBlkWidth();
  bin_width_ = region_width / grid_bin_cnt_x_;
  if (bin_width_ < blk_size_factor_ * average_blk_width) {
    bin_width_ = blk_size_factor_ * average_blk_width;
    grid_bin_cnt_x_ = static_cast<int>(std::round(region_width / bin_width_));
    bin_width_ = region_width / grid_bin_cnt_x_;
  }

  auto region_height = static_cast<double>(ckt_ptr_->RegionHeight());
  double average_blk_height = ckt_ptr_->AveMovBlkHeight();
  bin_height_ = region_height / grid_bin_cnt_y_;
  if (bin_height_ < blk_size_factor_ * average_blk_height) {
    bin_height_ = blk_size_factor_ * average_blk_height;
    grid_bin_cnt_y_ = static_cast<int>(std::round(region_height / bin_height_));
    bin_height_ = region_height / grid_bin_cnt_y_;
  }

  auto tmp_col = std::vector<InitializerGridBin>(grid_bin_cnt_y_);
  grid_bins_.assign(grid_bin_cnt_x_, tmp_col);
}

/****
 * @brief For each grid bin, store the list of blocks overlap with it.
 */
void MonteCarloInitializer::AssignFixedMacroToGridBin() {
  int region_llx = ckt_ptr_->RegionLLX();
  int region_urx = ckt_ptr_->RegionURX();
  int region_lly = ckt_ptr_->RegionLLY();
  int region_ury = ckt_ptr_->RegionURY();
  for (auto &&blk : ckt_ptr_->Blocks()) {
    // if this block is a fixed macro
    if (blk.IsMovable()) continue; // this condition may need to be updated in the future

    // if this block is out of the placement region, then ignore it
    if (blk.LLX() >= region_urx) continue;
    if (blk.LLY() >= region_ury) continue;
    if (blk.URX() <= region_llx) continue;
    if (blk.URY() <= region_lly) continue;

    // find the (x, y) index of the lower-left corner
    int lx_index = (int) std::floor((blk.LLX() - region_llx) / bin_width_);
    int ly_index = (int) std::floor((blk.LLY() - region_lly) / bin_height_);
    lx_index = std::max(lx_index, 0);
    ly_index = std::max(ly_index, 0);

    // find the (x, y) index of the upper-right corner
    int ux_index = (int) std::floor((blk.URX() - region_llx) / bin_width_);
    int uy_index = (int) std::floor((blk.URY() - region_lly) / bin_height_);
    ux_index = std::min(ux_index, grid_bin_cnt_x_ - 1);
    uy_index = std::min(uy_index, grid_bin_cnt_y_ - 1);

    // every grid bin overlaps with this fixed macro should cache this information for future reference
    for (int ix = lx_index; ix <= ux_index; ++ix) {
      for (int iy = ly_index; iy <= uy_index; ++iy) {
        // we can ignore the case where this fixed macro only touches the boundary of this grid bin.
        // but it is ok not to do it because this will only lead to a small performance penalty
        grid_bins_[ix][iy].Macros().push_back(&blk);
      }
    }
  }
}

/****
 * @brief Check if the center of this block falls into a fixed macro.
 *
 * @param blk: the block to be examined.
 * @return a boolean value indicate if the above check is passed or not.
 */
bool MonteCarloInitializer::IsBlkLocationValid(Block &blk) {
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();
  double x_loc = blk.X();
  double y_loc = blk.Y();
  int ix = static_cast<int>(std::floor((x_loc - region_llx) / bin_width_));
  int iy = static_cast<int>(std::floor((y_loc - region_lly) / bin_height_));
  auto &macros = grid_bins_[ix][iy].Macros();
  return std::all_of(
      macros.begin(),
      macros.end(),
      [&x_loc, &y_loc](const Block *macro_ptr) {
        return (x_loc >= macro_ptr->URX()) || (y_loc >= macro_ptr->URY())
            || (x_loc <= macro_ptr->LLX()) || (y_loc <= macro_ptr->LLY());
      }
  );
}

DensityAwareInitializer::DensityAwareInitializer(
    Circuit *ckt_ptr,
    unsigned int random_seed
) : MonteCarloInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "density-aware";
}

void DensityAwareInitializer::RandomPlace() {
  PrintStartStatement();

  InitializeGridBin();
  AssignFixedMacroToGridBin();
  InitializePriorityQueue();

  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();

  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  for (auto &&blk : blocks) {
    if (!blk.IsMovable()) continue;
    auto grid_bin = density_queue_.top();
    density_queue_.pop();
    grid_bin->AddBlock(&blk);
    density_queue_.emplace(grid_bin);
  }

  while (!density_queue_.empty()) {
    std::cout << density_queue_.top()->GetDensity() << "\n";
    density_queue_.pop();
  }

  PrintEndStatement();
}

void DensityAwareInitializer::InitializeGridBin() {
  MonteCarloInitializer::InitializeGridBin();

  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();
  for (int ix = 0; ix < grid_bin_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_bin_cnt_y_; ++iy) {
      double lx = region_llx + ix * bin_width_;
      double ly = region_lly + iy * bin_height_;
      double ux = lx + bin_width_;
      double uy = ly + bin_height_;
      grid_bins_[ix][iy].SetBoundary(lx, ly, ux, uy);
      grid_bins_[ix][iy].UpdateTotalArea();
    }
  }
}

void DensityAwareInitializer::AssignFixedMacroToGridBin() {
  MonteCarloInitializer::AssignFixedMacroToGridBin();
  /*
  for (int ix = 0; ix < grid_bin_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_bin_cnt_y_; ++iy) {
      grid_bins_[ix][iy].UpdateMacroArea();
    }
  }
   */
}

void DensityAwareInitializer::InitializePriorityQueue() {
  for (int ix = 0; ix < grid_bin_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_bin_cnt_y_; ++iy) {
      density_queue_.emplace(&(grid_bins_[ix][iy]));
    }
  }
  /*
  while (!density_queue_.empty()) {
    std::cout << density_queue_.top()->GetDensity() << "\n";
    density_queue_.pop();
  }

  std::cout << std::endl;
   */
}

} // dali
