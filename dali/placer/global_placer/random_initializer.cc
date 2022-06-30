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

#include <omp.h>

#include <algorithm>
#include <random>

#include "dali/common/logging.h"

namespace dali {

RandomInitializer::RandomInitializer(
    Circuit *ckt_ptr,
    int num_threads,
    unsigned int random_seed
) : ckt_ptr_(ckt_ptr),
    num_threads_(num_threads),
    random_seed_(random_seed) {
  DaliExpects(ckt_ptr_ != nullptr, "Ckt is a null ptr?");
}

void RandomInitializer::PrintStartStatement() {
  elapsed_time_.RecordStartTime();
  BOOST_LOG_TRIVIAL(info)
    << "  HPWL before random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";
}

void RandomInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(info)
    << "  HPWL after random initialization: "
    << ckt_ptr_->WeightedHPWL() << "\n";
  if (should_save_intermediate_result_) {
    ckt_ptr_->GenMATLABTable("rand_init.txt");
  }
}

void UniformInitializer::RandomPlace() {
  PrintStartStatement();
  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_left = ckt_ptr_->RegionLLX();
  int region_bottom = ckt_ptr_->RegionLLY();
  for (auto &blk : blocks) {
    if (!blk.IsMovable()) continue;
    double init_x = region_left + region_width * distribution(generator);
    double init_y = region_bottom + region_height * distribution(generator);
    blk.SetCenterX(init_x);
    blk.SetCenterY(init_y);
  }

  PrintEndStatement();
}

void UniformInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(debug)
    << "  block location uniform initialization complete\n";
  RandomInitializer::PrintEndStatement();
}

void NormalInitializer::RandomPlace() {
  PrintStartStatement();
  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::normal_distribution<double> normal_distribution(0.0, std_dev_);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_left = ckt_ptr_->RegionLLX();
  int region_right = ckt_ptr_->RegionURX();
  int region_bottom = ckt_ptr_->RegionLLY();
  int region_top = ckt_ptr_->RegionURY();
  double region_center_x = (region_right + region_left) / 2.0;
  double region_center_y = (region_top + region_bottom) / 2.0;
  for (auto &block : blocks) {
    if (!block.IsMovable()) continue;
    double x = region_center_x + region_width * normal_distribution(generator);
    double y = region_center_y + region_height * normal_distribution(generator);
    x = std::max(x, (double) region_left);
    x = std::min(x, (double) region_right);
    y = std::max(y, (double) region_bottom);
    y = std::min(y, (double) region_top);
    block.SetCenterX(x);
    block.SetCenterY(y);
  }

  PrintEndStatement();
}

void NormalInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(debug)
    << "  block location gaussian initialization complete\n";
  RandomInitializer::PrintEndStatement();
}

} // dali
