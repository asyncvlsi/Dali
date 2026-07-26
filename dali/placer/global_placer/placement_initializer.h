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
#ifndef DALI_PLACER_GLOBAL_PLACER_PLACEMENT_INITIALIZER_H_
#define DALI_PLACER_GLOBAL_PLACER_PLACEMENT_INITIALIZER_H_

#include <queue>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include "dali/circuit/circuit.h"
#include "dali/circuit/component.h"
#include "dali/common/elapsed_time.h"

namespace dali {

/** Available component-location initialization strategies. */
enum class PlacementInitializerType {
  /** Preserve locations loaded from the input design. */
  kKeep = -1,
  kUniform = 0,
  kGaussian = 1,
  kMonteCarlo = 2,
  kDensityAware = 3
};

/** Interface for strategies that assign movable-component starting locations.
 */
class PlacementInitializer {
 public:
  PlacementInitializer(Circuit* ckt_ptr, uint32_t random_seed);
  virtual ~PlacementInitializer() = default;

  /** Set initializer-specific parameters parsed from configuration. */
  virtual void SetParameters(
      std::unordered_map<std::string, std::string>& params_dict);

  /** Enable or disable intermediate placement dumps. */

  /** Assign initial component locations. */
  virtual void InitializeLocations() = 0;

 protected:
  void PrintStartStatement();
  void PrintEndStatement();
  Circuit* ckt_ptr_ = nullptr;
  uint32_t random_seed_ = 1;

  // Save intermediate result for debugging and/or visualization.

  ElapsedTime elapsed_time_;
  std::string initializer_name_;
};

/** Uniformly places cells across the placement region without size awareness.
 */
class UniformInitializer : public PlacementInitializer {
 public:
  /** Scatter components uniformly across the region. */
  explicit UniformInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~UniformInitializer() override = default;
  void InitializeLocations() override;
};

/** Places cells with a normal distribution centered on the placement region. */
class GaussianInitializer : public PlacementInitializer {
 public:
  /** Scatter components about the region centre with a Gaussian spread. */
  explicit GaussianInitializer(Circuit* ckt_ptr, uint32_t = 1);
  ~GaussianInitializer() override = default;
  void SetParameters(
      std::unordered_map<std::string, std::string>& params_dict) override;
  void InitializeLocations() override;

 protected:
  double std_dev_ = 1.0 / 3.0;
};

/** Grid bin used to avoid fixed macros during placement initialization. */
class InitializerGridBin {
 public:
  std::vector<Component*>& Macros();
  double GetDensity() const;
  double PriorityTieBreaker() const { return priority_tie_breaker_; }
  void UpdateDensity();
  /** Set the region the initializer scatters within. */
  void SetBoundary(int lx, int ly, int ux, int uy);
  void SetPriorityTieBreaker(double priority_tie_breaker);
  void UpdateTotalArea();
  /** Rebuild legal free rectangles after fixed macros are assigned. */
  void UpdateFreeSpace();
  /** Register a component to be given a starting location. */
  void AddComponent(Component* component);
  /** Seed assigned components across legal free rectangles. */
  void InitializeComponentLocation(uint32_t random_seed, int num_trials);

 private:
  /** Return rectangle area using the unsigned type used by density accounting.
   */
  unsigned long long RectangleArea(const RectI& rect) const;

  /** Slice this bin by macro edges and keep only unblocked sub-rectangles. */
  void BuildFreeRectangles(std::vector<RectI> const& blocked_rects);

  std::vector<Component*> macros_;
  std::vector<Component*> components_;
  std::vector<RectI> free_rects_;
  double density_ = 0;
  double priority_tie_breaker_ = 0;
  unsigned long long total_area_ = 0;
  unsigned long long free_area_ = 0;
  unsigned long long movable_area_ = 0;
  int lx_ = 0;
  int ly_ = 0;
  int ux_ = 0;
  int uy_ = 0;
};

struct CompareInitializerGridBinPtr {
  bool operator()(InitializerGridBin const* p1, InitializerGridBin const* p2) {
    if (p1->GetDensity() != p2->GetDensity()) {
      return p1->GetDensity() > p2->GetDensity();
    }
    return p1->PriorityTieBreaker() > p2->PriorityTieBreaker();
  }
};

/** Uniform random initializer that rejects locations overlapping fixed macros.
 */
class MonteCarloInitializer : public PlacementInitializer {
 public:
  /** Place components by random trials, keeping the best by wirelength. */
  explicit MonteCarloInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~MonteCarloInitializer() override = default;
  void InitializeLocations() override;

 protected:
  virtual void InitializeGridBin();
  virtual void AssignFixedMacroToGridBin();
  bool IsComponentLocationValid(Component& component);

  int grid_cnt_x_ = 30;
  int grid_cnt_y_ = 30;
  int bin_width_ = 0;
  int bin_height_ = 0;
  int component_size_factor_ = 5;
  std::vector<std::vector<InitializerGridBin>> grid_bins_;
  int num_trials_ = 50;
};

/** Density-aware initializer that places each component in the least dense bin.
 */
class DensityAwareInitializer : public MonteCarloInitializer {
 public:
  /** Seed locations from a coarse density estimate, spreading dense areas. */
  explicit DensityAwareInitializer(Circuit* ckt_ptr, uint32_t random_seed = 1);
  ~DensityAwareInitializer() override = default;
  void InitializeLocations() override;

 protected:
  void InitializeGridBin() override;
  void AssignFixedMacroToGridBin() override;

  std::priority_queue<InitializerGridBin*, std::vector<InitializerGridBin*>,
                      CompareInitializerGridBinPtr>
      density_queue_;
  void InitializePriorityQueue();
  void AssignComponentToGridBin();
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_PLACEMENT_INITIALIZER_H_
