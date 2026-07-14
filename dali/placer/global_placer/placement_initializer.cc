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

#include "placement_initializer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {

static double ClampCenterToBox(double center, double lower, double upper,
                               double object_size) {
  double min_center = lower + object_size / 2.0;
  double max_center = upper - object_size / 2.0;
  if (min_center > max_center) {
    return (lower + upper) / 2.0;
  }
  return std::clamp(center, min_center, max_center);
}

static double HaltonFraction(int index, int base) {
  DaliExpects(index > 0, "Halton sequence index must be positive");
  DaliExpects(base > 1, "Halton sequence base must be larger than one");
  double fraction = 1.0;
  double result = 0.0;
  while (index > 0) {
    fraction /= base;
    result += fraction * (index % base);
    index /= base;
  }
  return result;
}

PlacementInitializer::PlacementInitializer(Circuit* ckt_ptr,
                                           uint32_t random_seed)
    : ckt_ptr_(ckt_ptr), random_seed_(random_seed) {
  DaliExpects(ckt_ptr_ != nullptr, "Ckt is a null ptr?");
  initializer_name_ = "placement";
}

void PlacementInitializer::SetShouldSaveIntermediateResult(
    bool should_save_intermediate_result) {
  should_save_intermediate_result_ = should_save_intermediate_result;
}

void PlacementInitializer::PrintStartStatement() {
  elapsed_time_.RecordStartTime();
  RecordPlacementHpwlMetrics("initialization.before", *ckt_ptr_);
  LOG(info) << "  Component location initialization:\n"
            << "    HPWL before, " << ckt_ptr_->WeightedHPWL() << "\n";
}

void PlacementInitializer::SetParameters(
    [[maybe_unused]] std::unordered_map<std::string, std::string>&
        params_dict) {}

void PlacementInitializer::PrintEndStatement() {
  LOG(debug) << "    " << initializer_name_ << " initialization complete\n";
  RecordPlacementHpwlMetrics("initialization.after", *ckt_ptr_);
  LOG(info) << "    HPWL after, " << ckt_ptr_->WeightedHPWL() << "\n";
  elapsed_time_.RecordEndTime();
  elapsed_time_.PrintTimeElapsed(severity::debug);
  if (should_save_intermediate_result_) {
    ckt_ptr_->GenMATLABTable("rand_init.txt");
  }
}

UniformInitializer::UniformInitializer(Circuit* ckt_ptr, uint32_t random_seed)
    : PlacementInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "uniform";
}

void UniformInitializer::InitializeLocations() {
  PrintStartStatement();

  int region_width = ckt_ptr_->RegionWidth();
  int region_height = ckt_ptr_->RegionHeight();
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();

  // initialize the random number generator
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Component>& components = ckt_ptr_->Components();
  for (auto& component : components) {
    if (!component.IsMovable()) continue;
    double init_x = region_llx + region_width * distribution(generator);
    double init_y = region_lly + region_height * distribution(generator);
    init_x = ClampCenterToBox(init_x, region_llx, region_llx + region_width,
                              component.Width());
    init_y = ClampCenterToBox(init_y, region_lly, region_lly + region_height,
                              component.Height());
    component.SetCenterX(init_x);
    component.SetCenterY(init_y);
  }

  PrintEndStatement();
}

GaussianInitializer::GaussianInitializer(Circuit* ckt_ptr, uint32_t random_seed)
    : PlacementInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "Gaussian";
}

void GaussianInitializer::SetParameters(
    std::unordered_map<std::string, std::string>& params_dict) {
  std::string std_dev_name = "std_dev";
  if (params_dict.find(std_dev_name) != params_dict.end()) {
    try {
      std_dev_ = std::stod(params_dict.at(std_dev_name));
    } catch (...) {
      DaliFatal("Failed to convert " << params_dict.at(std_dev_name)
                                     << " to a double");
    }
  }
}

void GaussianInitializer::InitializeLocations() {
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
  for (auto& component : ckt_ptr_->Components()) {
    if (!component.IsMovable()) continue;
    double x = center_x + region_width * normal_distribution(generator);
    double y = center_y + region_height * normal_distribution(generator);
    x = ClampCenterToBox(x, region_llx, region_urx, component.Width());
    y = ClampCenterToBox(y, region_lly, region_ury, component.Height());
    component.SetCenterX(x);
    component.SetCenterY(y);
  }

  PrintEndStatement();
}

std::vector<Component*>& InitializerGridBin::Macros() { return macros_; }

double InitializerGridBin::GetDensity() const { return density_; }

void InitializerGridBin::UpdateDensity() {
  if (free_area_ == 0) {
    density_ = std::numeric_limits<double>::infinity();
    return;
  }
  density_ =
      static_cast<double>(movable_area_) / static_cast<double>(free_area_);
}

void InitializerGridBin::SetBoundary(int lx, int ly, int ux, int uy) {
  lx_ = lx;
  ly_ = ly;
  ux_ = ux;
  uy_ = uy;
}

void InitializerGridBin::SetPriorityTieBreaker(double priority_tie_breaker) {
  priority_tie_breaker_ = priority_tie_breaker;
}

void InitializerGridBin::UpdateTotalArea() {
  total_area_ = (ux_ - lx_) * (uy_ - ly_);
}

unsigned long long InitializerGridBin::RectangleArea(const RectI& rect) const {
  return static_cast<unsigned long long>(rect.Width()) *
         static_cast<unsigned long long>(rect.Height());
}

void InitializerGridBin::BuildFreeRectangles(
    std::vector<RectI> const& blocked_rects) {
  free_rects_.clear();
  std::vector<int> x_lines = {lx_, ux_};
  std::vector<int> y_lines = {ly_, uy_};
  for (const RectI& rect : blocked_rects) {
    x_lines.push_back(rect.LLX());
    x_lines.push_back(rect.URX());
    y_lines.push_back(rect.LLY());
    y_lines.push_back(rect.URY());
  }
  std::sort(x_lines.begin(), x_lines.end());
  x_lines.erase(std::unique(x_lines.begin(), x_lines.end()), x_lines.end());
  std::sort(y_lines.begin(), y_lines.end());
  y_lines.erase(std::unique(y_lines.begin(), y_lines.end()), y_lines.end());

  free_area_ = 0;
  for (std::size_t ix = 1; ix < x_lines.size(); ++ix) {
    for (std::size_t iy = 1; iy < y_lines.size(); ++iy) {
      RectI candidate(x_lines[ix - 1], y_lines[iy - 1], x_lines[ix],
                      y_lines[iy]);
      if (candidate.Width() == 0 || candidate.Height() == 0) continue;

      bool blocked = std::any_of(blocked_rects.begin(), blocked_rects.end(),
                                 [&candidate](const RectI& rect) {
                                   return candidate.IsOverlap(rect);
                                 });
      if (!blocked) {
        free_area_ += RectangleArea(candidate);
        free_rects_.push_back(candidate);
      }
    }
  }
}

void InitializerGridBin::UpdateFreeSpace() {
  RectI bin_rect(lx_, ly_, ux_, uy_);
  std::vector<RectI> blocked_rects;
  for (auto& macro_ptr : macros_) {
    DaliExpects(macro_ptr->IsFixed(), "Only supports fixed macros");
    RectI fixed_component_rect(static_cast<int>(std::round(macro_ptr->LLX())),
                               static_cast<int>(std::round(macro_ptr->LLY())),
                               static_cast<int>(std::round(macro_ptr->URX())),
                               static_cast<int>(std::round(macro_ptr->URY())));
    if (bin_rect.IsOverlap(fixed_component_rect)) {
      blocked_rects.push_back(bin_rect.GetOverlapRect(fixed_component_rect));
    }
  }

  BuildFreeRectangles(blocked_rects);
  UpdateDensity();
}

void InitializerGridBin::AddComponent(Component* component) {
  components_.emplace_back(component);
  movable_area_ += component->Area();
  UpdateDensity();
}

void InitializerGridBin::InitializeComponentLocation(uint32_t random_seed,
                                                     int num_trials) {
  (void)num_trials;

  if (free_rects_.empty()) return;

  int movable_component_count = 0;
  for (auto& component_ptr : components_) {
    if (component_ptr->IsMovable()) {
      ++movable_component_count;
    }
  }
  if (movable_component_count == 0) return;

  int sequence_offset = static_cast<int>(random_seed % 997) * 997;
  int movable_component_index = 0;
  for (auto& component_ptr : components_) {
    if (!component_ptr->IsMovable()) continue;
    std::vector<const RectI*> candidate_rects;
    std::vector<unsigned long long> cumulative_area;
    unsigned long long total_candidate_area = 0;
    for (const RectI& rect : free_rects_) {
      if (rect.Width() < component_ptr->Width() ||
          rect.Height() < component_ptr->Height()) {
        continue;
      }
      total_candidate_area += RectangleArea(rect);
      cumulative_area.push_back(total_candidate_area);
      candidate_rects.push_back(&rect);
    }
    if (candidate_rects.empty()) continue;

    int sample_index = sequence_offset + movable_component_index + 1;
    double area_fraction = HaltonFraction(sample_index, 5);
    unsigned long long area_sample =
        std::min(static_cast<unsigned long long>(
                     area_fraction * static_cast<double>(total_candidate_area)),
                 total_candidate_area - 1);
    auto rect_iter = std::upper_bound(cumulative_area.begin(),
                                      cumulative_area.end(), area_sample);
    int rect_index =
        static_cast<int>(std::distance(cumulative_area.begin(), rect_iter));
    const RectI& free_rect = *candidate_rects[rect_index];

    double x_fraction = HaltonFraction(sample_index, 2);
    double y_fraction = HaltonFraction(sample_index, 3);
    double x_loc = free_rect.LLX() + free_rect.Width() * x_fraction;
    double y_loc = free_rect.LLY() + free_rect.Height() * y_fraction;
    x_loc = ClampCenterToBox(x_loc, free_rect.LLX(), free_rect.URX(),
                             component_ptr->Width());
    y_loc = ClampCenterToBox(y_loc, free_rect.LLY(), free_rect.URY(),
                             component_ptr->Height());
    component_ptr->SetCenterX(x_loc);
    component_ptr->SetCenterY(y_loc);
    ++movable_component_index;
  }
}

MonteCarloInitializer::MonteCarloInitializer(Circuit* ckt_ptr,
                                             uint32_t random_seed)
    : PlacementInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "Monte Carlo";
}

void MonteCarloInitializer::InitializeLocations() {
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

  std::vector<Component>& components = ckt_ptr_->Components();
  for (auto& component : components) {
    if (!component.IsMovable()) continue;
    for (int i = 0; i < num_trials_; ++i) {
      double init_x = region_llx + region_width * distribution(generator);
      double init_y = region_lly + region_height * distribution(generator);
      init_x = ClampCenterToBox(init_x, region_llx, region_llx + region_width,
                                component.Width());
      init_y = ClampCenterToBox(init_y, region_lly, region_lly + region_height,
                                component.Height());
      component.SetCenterX(init_x);
      component.SetCenterY(init_y);
      if (IsComponentLocationValid(component)) {
        break;
      }
    }
  }

  PrintEndStatement();
}

/****
 * @brief Initialize a grid bin to store fixed macros in each bin
 */
void MonteCarloInitializer::InitializeGridBin() {
  int region_width = ckt_ptr_->RegionWidth();
  double average_component_width = ckt_ptr_->AverageMovableComponentWidth();
  bin_width_ = std::max(1, region_width / grid_cnt_x_);
  if (bin_width_ < component_size_factor_ * average_component_width) {
    bin_width_ = std::ceil(component_size_factor_ * average_component_width);
    bin_width_ = std::max(bin_width_, 1);
    grid_cnt_x_ = std::max(
        1, static_cast<int>(
               std::ceil(region_width / static_cast<double>(bin_width_))));
  }

  int region_height = ckt_ptr_->RegionHeight();
  double average_component_height = ckt_ptr_->AverageMovableComponentHeight();
  bin_height_ = std::max(1, region_height / grid_cnt_y_);
  if (bin_height_ < component_size_factor_ * average_component_height) {
    bin_height_ = std::ceil(component_size_factor_ * average_component_height);
    bin_height_ = std::max(bin_height_, 1);
    grid_cnt_y_ = std::max(
        1, static_cast<int>(
               std::ceil(region_height / static_cast<double>(bin_height_))));
  }

  auto tmp_col = std::vector<InitializerGridBin>(grid_cnt_y_);
  grid_bins_.assign(grid_cnt_x_, tmp_col);
}

/****
 * @brief For each grid bin, store the list of components overlap with it.
 */
void MonteCarloInitializer::AssignFixedMacroToGridBin() {
  int region_llx = ckt_ptr_->RegionLLX();
  int region_urx = ckt_ptr_->RegionURX();
  int region_lly = ckt_ptr_->RegionLLY();
  int region_ury = ckt_ptr_->RegionURY();
  for (auto& component : ckt_ptr_->Components()) {
    // skip movable components, this condition may need to be updated in the
    // future
    if (component.IsMovable()) continue;

    // skip components out of the placement region
    if (component.LLX() >= region_urx) continue;
    if (component.LLY() >= region_ury) continue;
    if (component.URX() <= region_llx) continue;
    if (component.URY() <= region_lly) continue;

    // find the (x, y) index of the lower-left corner
    int lx_index = std::floor((component.LLX() - region_llx) / bin_width_);
    int ly_index = std::floor((component.LLY() - region_lly) / bin_height_);
    lx_index = std::max(lx_index, 0);
    ly_index = std::max(ly_index, 0);

    // find the (x, y) index of the upper-right corner
    int ux_index = std::floor((component.URX() - region_llx) / bin_width_);
    int uy_index = std::floor((component.URY() - region_lly) / bin_height_);
    ux_index = std::min(ux_index, grid_cnt_x_ - 1);
    uy_index = std::min(uy_index, grid_cnt_y_ - 1);

    // every grid bin overlaps with this fixed macro should cache this
    // information for future reference
    for (int ix = lx_index; ix <= ux_index; ++ix) {
      for (int iy = ly_index; iy <= uy_index; ++iy) {
        // we can ignore the case where this fixed macro only touches the
        // boundary of this grid bin. but it is ok not to do it because this
        // will only lead to a small performance penalty
        grid_bins_[ix][iy].Macros().push_back(&component);
      }
    }
  }
}

/** Return true if the current component rectangle avoids nearby fixed macros.
 */
bool MonteCarloInitializer::IsComponentLocationValid(Component& component) {
  int region_llx = ckt_ptr_->RegionLLX();
  int region_lly = ckt_ptr_->RegionLLY();
  double x_loc = component.X();
  double y_loc = component.Y();
  int ix = std::floor((x_loc - region_llx) / bin_width_);
  int iy = std::floor((y_loc - region_lly) / bin_height_);
  ix = std::max(ix, 0);
  ix = std::min(ix, grid_cnt_x_ - 1);
  iy = std::max(iy, 0);
  iy = std::min(iy, grid_cnt_y_ - 1);
  auto& macros = grid_bins_[ix][iy].Macros();
  return std::all_of(macros.begin(), macros.end(),
                     [&component](const Component* macro_ptr) {
                       return (component.LLX() >= macro_ptr->URX()) ||
                              (component.LLY() >= macro_ptr->URY()) ||
                              (component.URX() <= macro_ptr->LLX()) ||
                              (component.URY() <= macro_ptr->LLY());
                     });
}

DensityAwareInitializer::DensityAwareInitializer(Circuit* ckt_ptr,
                                                 uint32_t random_seed)
    : MonteCarloInitializer(ckt_ptr, random_seed) {
  initializer_name_ = "density-aware";
}

void DensityAwareInitializer::InitializeLocations() {
  PrintStartStatement();

  InitializeGridBin();
  AssignFixedMacroToGridBin();
  InitializePriorityQueue();
  AssignComponentToGridBin();

  for (int ix = 0; ix < grid_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_cnt_y_; ++iy) {
      grid_bins_[ix][iy].InitializeComponentLocation(
          random_seed_ + ix * 10 + iy, num_trials_);
    }
  }

  PrintEndStatement();
}

void DensityAwareInitializer::InitializeGridBin() {
  MonteCarloInitializer::InitializeGridBin();

  int region_lx = ckt_ptr_->RegionLLX();
  int region_ly = ckt_ptr_->RegionLLY();
  int region_ux = ckt_ptr_->RegionURX();
  int region_uy = ckt_ptr_->RegionURY();
  std::minstd_rand0 generator{random_seed_};
  std::uniform_real_distribution<double> distribution(0, 1);
  for (int ix = 0; ix < grid_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_cnt_y_; ++iy) {
      int lx = region_lx + ix * bin_width_;
      int ly = region_ly + iy * bin_height_;
      int ux = lx + bin_width_;
      int uy = ly + bin_height_;
      ux = ix == grid_cnt_x_ - 1 ? region_ux : std::min(ux, region_ux);
      uy = iy == grid_cnt_y_ - 1 ? region_uy : std::min(uy, region_uy);
      grid_bins_[ix][iy].SetBoundary(lx, ly, ux, uy);
      grid_bins_[ix][iy].SetPriorityTieBreaker(distribution(generator));
      grid_bins_[ix][iy].UpdateTotalArea();
    }
  }
}

void DensityAwareInitializer::AssignFixedMacroToGridBin() {
  MonteCarloInitializer::AssignFixedMacroToGridBin();
  for (int ix = 0; ix < grid_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_cnt_y_; ++iy) {
      grid_bins_[ix][iy].UpdateFreeSpace();
    }
  }
}

void DensityAwareInitializer::InitializePriorityQueue() {
  decltype(density_queue_) empty_queue;
  density_queue_.swap(empty_queue);
  for (int ix = 0; ix < grid_cnt_x_; ++ix) {
    for (int iy = 0; iy < grid_cnt_y_; ++iy) {
      density_queue_.emplace(&(grid_bins_[ix][iy]));
    }
  }
}

void DensityAwareInitializer::AssignComponentToGridBin() {
  std::vector<Component*> movable_components;
  movable_components.reserve(ckt_ptr_->Components().size());
  for (auto& component : ckt_ptr_->Components()) {
    if (component.IsMovable()) {
      movable_components.push_back(&component);
    }
  }
  std::minstd_rand0 generator{random_seed_};
  std::shuffle(movable_components.begin(), movable_components.end(), generator);

  for (Component* component : movable_components) {
    auto grid_bin = density_queue_.top();
    density_queue_.pop();
    grid_bin->AddComponent(component);
    density_queue_.emplace(grid_bin);
  }
}

}  // namespace dali
