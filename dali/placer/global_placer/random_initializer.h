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
#ifndef DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_
#define DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_

#include <unordered_map>
#include <string>

#include "dali/circuit/block.h"
#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"

/****
 * This header file contains classes for initializing cell location at random.
 * See each class for more information.
 */

namespace dali {

/****
 * This class contains possible initializers which users can choose from.
 */
enum class RandomInitializerType {
  UNIFORM = 0,
  GAUSSIAN = 1,
  MONTE_CARLO = 2,
  DENSITY_AWARE = 3
};

/****
 * This is an abstract class defining the interface of all random initializers.
 */
class RandomInitializer {
 public:
  RandomInitializer(Circuit *ckt_ptr, unsigned int random_seed);
  virtual ~RandomInitializer() = default;
  virtual void SetParameters(
      std::unordered_map<std::string, std::string> &params_dict
  );
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);
  virtual void RandomPlace() = 0;
 protected:
  virtual void PrintStartStatement();
  virtual void PrintEndStatement();
  Circuit *ckt_ptr_ = nullptr;
  unsigned int random_seed_ = 1;

  // save intermediate result for debugging and/or visualization
  bool should_save_intermediate_result_ = false;

  ElapsedTime elapsed_time_;
};

/****
 * This class implements an initializer that uniformly place cells across the
 * whole placement region at random without considering the size of cells.
 */
class UniformInitializer : public RandomInitializer {
 public:
  explicit UniformInitializer(
      Circuit *ckt_ptr,
      unsigned int random_seed = 1
  ) : RandomInitializer(ckt_ptr, random_seed) {}
  ~UniformInitializer() override = default;
  void RandomPlace() override;
 protected:
  void PrintEndStatement() override;

};

/****
 * This class implements an initializer that randomly place cells across the
 * whole placement region following the normal distribution without considering
 * the size of cells.
 * The center of the distribution is the center of the placement region, which
 * is assumed to be a rectangle.
 * The deviation of this distribution will be scaled by the region width and height.
 */
class GaussianInitializer : public RandomInitializer {
 public:
  explicit GaussianInitializer(
      Circuit *ckt_ptr,
      unsigned int random_seed = 1
  ) : RandomInitializer(ckt_ptr, random_seed) {}
  ~GaussianInitializer() override = default;
  void SetParameters(
      std::unordered_map<std::string, std::string> &params_dict
  ) override;
  void RandomPlace() override;
 protected:
  void PrintEndStatement() override;
  double std_dev_ = 1.0 / 3.0;
};

/****
 * This class implements an initializer that uniformly place cells across the
 * whole white space at random without considering the size of cells.
 * This is very similar to UniformInitializer, the difference is that if the
 * random location of a cell is on the top of a fixed macro, then re-generate
 * a random location.
 *
 * Note: this initializer is designed for the scenario where all macros are
 * fixed (not unplaced or placed). It may not work very well for other scenarios.
 */
class MonteCarloInitializer : public RandomInitializer {
 public:
  explicit MonteCarloInitializer(
      Circuit *ckt_ptr,
      unsigned int random_seed = 1
  ) : RandomInitializer(ckt_ptr, random_seed) {}
  ~MonteCarloInitializer() override = default;
  void RandomPlace() override;
 protected:
  void PrintEndStatement() override;

  void InitializeGridBin();
  void AssignFixedMacroToGridBin();
  bool IsBlkLocationValid(Block &blk);
  int grid_bin_cnt_x_ = 30;
  int grid_bin_cnt_y_ = 30;
  double bin_width_ = 0;
  double bin_height_ = 0;
  int blk_size_factor_ = 5;
  // contiguous memory layout is not very important in this case, use a vector
  // of vector to simulate the 2-D array
  std::vector<std::vector<std::vector<Block *>>> macros_in_grid_bin_;
  int num_trials_ = 50;
};

/****
 * This class implements an initializer that uniformly place cells across the
 * whole white space at random.
 * This initializer will:
 *   1. divide the placement region into many grid bins;
 *   2. for each cell, find the grid bin with the smallest cell density;
 *
 * By default, a 100 * 100 mesh is used to divide the placement region into
 * grid bins. The grid bin width/height is at least 10 times of average cell
 * width/height, and thus the number of grid bins might be adjusted.
 * Future work: we can make it possible to set these parameters by users.
 *
 * Note: this initializer is designed for the scenario where all macros are
 * fixed (not unplaced or placed). It may not work very well for other scenarios.
 */
class DensityAwareInitializer : public RandomInitializer {
 public:
  explicit DensityAwareInitializer(
      Circuit *ckt_ptr,
      unsigned int random_seed = 1
  ) : RandomInitializer(ckt_ptr, random_seed) {}
  ~DensityAwareInitializer() override = default;
  void RandomPlace() override;
 protected:
  void PrintEndStatement() override;
};

} // dali

#endif //DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_
