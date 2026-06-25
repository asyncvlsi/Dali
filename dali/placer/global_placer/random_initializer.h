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

#include <queue>
#include <string>
#include <unordered_map>

#include "dali/circuit/block.h"
#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"

namespace dali {

/** Available random-placement initialization strategies. */
enum class RandomInitializerType {
  UNIFORM = 0,
  GAUSSIAN = 1,
  MONTE_CARLO = 2,
  DENSITY_AWARE = 3
};

/** Interface for random initializers that seed block locations. */
class RandomInitializer {
 public:
  RandomInitializer(Circuit* ckt_ptr, uint32_t random_seed);
  virtual ~RandomInitializer() = default;

  /** Set initializer-specific parameters parsed from configuration. */
  virtual void SetParameters(
      std::unordered_map<std::string, std::string>& params_dict);

  /** Enable or disable intermediate placement dumps. */
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);

  /** Assign initial block locations. */
  virtual void RandomPlace() = 0;

 protected:
  void PrintStartStatement();
  void PrintEndStatement();
  Circuit* ckt_ptr_ = nullptr;
  uint32_t random_seed_ = 1;

  // Save intermediate result for debugging and/or visualization.
  bool should_save_intermediate_result_ = false;

  ElapsedTime elapsed_time_;
  std::string initializer_name_;
};

/** Uniformly places cells across the placement region without size awareness.
 */
class UniformInitializer : public RandomInitializer {
 public:
  explicit UniformInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~UniformInitializer() override = default;
  void RandomPlace() override;
};

/** Places cells with a normal distribution centered on the placement region. */
class GaussianInitializer : public RandomInitializer {
 public:
  explicit GaussianInitializer(Circuit* ckt_ptr, uint32_t = 1);
  ~GaussianInitializer() override = default;
  void SetParameters(
      std::unordered_map<std::string, std::string>& params_dict) override;
  void RandomPlace() override;

 protected:
  double std_dev_ = 1.0 / 3.0;
};

/** Grid bin used to avoid fixed macros during random initialization. */
class InitializerGridBin {
 public:
  std::vector<Block*>& Macros();
  double GetDensity() const;
  void UpdateDensity();
  void SetBoundary(int lx, int ly, int ux, int uy);
  void UpdateTotalArea();
  void UpdateMacroArea();
  void AddBlock(Block* blk);
  void InitializeBlockLocation(uint32_t random_seed, int num_trials);

 private:
  std::vector<Block*> macros_;
  std::vector<Block*> blocks_;
  double density_ = 0;
  unsigned long long total_area_ = 0;
  unsigned long long used_area_ = 0;
  int lx_ = 0;
  int ly_ = 0;
  int ux_ = 0;
  int uy_ = 0;
};

struct CompareInitializerGridBinPtr {
  bool operator()(InitializerGridBin const* p1, InitializerGridBin const* p2) {
    return p1->GetDensity() > p2->GetDensity();
  }
};

/** Uniform random initializer that rejects locations overlapping fixed macros.
 */
class MonteCarloInitializer : public RandomInitializer {
 public:
  explicit MonteCarloInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~MonteCarloInitializer() override = default;
  void RandomPlace() override;

 protected:
  virtual void InitializeGridBin();
  virtual void AssignFixedMacroToGridBin();
  bool IsBlkLocationValid(Block& blk);

  int grid_cnt_x_ = 30;
  int grid_cnt_y_ = 30;
  int bin_width_ = 0;
  int bin_height_ = 0;
  int blk_size_factor_ = 5;
  std::vector<std::vector<InitializerGridBin>> grid_bins_;
  int num_trials_ = 50;
};

/** Density-aware initializer that places each cell in the least dense bin. */
class DensityAwareInitializer : public MonteCarloInitializer {
 public:
  explicit DensityAwareInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~DensityAwareInitializer() override = default;
  void RandomPlace() override;

 protected:
  void InitializeGridBin() override;
  void AssignFixedMacroToGridBin() override;

  std::priority_queue<InitializerGridBin*, std::vector<InitializerGridBin*>,
                      CompareInitializerGridBinPtr>
      density_queue_;
  void InitializePriorityQueue();
  void AssignBlockToGridBin();
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_
