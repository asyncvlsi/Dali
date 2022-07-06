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
    unsigned int random_seed
) : ckt_ptr_(ckt_ptr),
    random_seed_(random_seed) {
  DaliExpects(ckt_ptr_ != nullptr, "Ckt is a null ptr?");
}

void RandomInitializer::SetShouldSaveIntermediateResult(bool should_save_intermediate_result) {
  should_save_intermediate_result_ = should_save_intermediate_result;
}

void RandomInitializer::PrintStartStatement() {
  elapsed_time_.RecordStartTime();
  BOOST_LOG_TRIVIAL(info)
    << "  HPWL before random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";
}

void RandomInitializer::SetParameters(
    [[maybe_unused]]std::unordered_map<std::string, std::string> &params_dict
) {}

void RandomInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(info)
    << "  HPWL after random initialization: "
    << ckt_ptr_->WeightedHPWL() << "\n";
  elapsed_time_.RecordEndTime();
  elapsed_time_.PrintTimeElapsed(boost::log::trivial::debug);
  if (should_save_intermediate_result_) {
    ckt_ptr_->GenMATLABTable("rand_init.txt");
  }
}

void UniformInitializer::RandomPlace() {
  PrintStartStatement();

  int width = ckt_ptr_->RegionWidth();
  int height = ckt_ptr_->RegionHeight();
  int llx = ckt_ptr_->RegionLLX();
  int lly = ckt_ptr_->RegionLLY();

  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  for (auto &&blk : blocks) {
    if (!blk.IsMovable()) continue;
    double init_x = llx + width * distribution(generator);
    double init_y = lly + height * distribution(generator);
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

void NormalInitializer::SetParameters(
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

void NormalInitializer::RandomPlace() {
  PrintStartStatement();
  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::normal_distribution<double> normal_distribution(0.0, std_dev_);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int width = ckt_ptr_->RegionWidth();
  int height = ckt_ptr_->RegionHeight();
  int llx = ckt_ptr_->RegionLLX();
  int urx = ckt_ptr_->RegionURX();
  int lly = ckt_ptr_->RegionLLY();
  int ury = ckt_ptr_->RegionURY();
  double center_x = (urx + llx) / 2.0;
  double center_y = (ury + lly) / 2.0;
  for (auto &blk : blocks) {
    if (!blk.IsMovable()) continue;
    double x = center_x + width * normal_distribution(generator);
    double y = center_y + height * normal_distribution(generator);
    x = std::max(x, (double) llx);
    x = std::min(x, (double) urx);
    y = std::max(y, (double) lly);
    y = std::min(y, (double) ury);
    blk.SetCenterX(x);
    blk.SetCenterY(y);
  }

  PrintEndStatement();
}

void NormalInitializer::PrintEndStatement() {
  BOOST_LOG_TRIVIAL(debug)
    << "  block location gaussian initialization complete\n";
  RandomInitializer::PrintEndStatement();
}

void MonteCarloInitializer::RandomPlace() {
  DaliFatal("Not implemented");
}

void MonteCarloInitializer::PrintEndStatement() {
  DaliFatal("Not implemented");
}

} // dali
