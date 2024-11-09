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
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_

#include <vector>

#include "dali/placer/global_placer/hpwl_optimizer.h"
#include "dali/placer/global_placer/random_initializer.h"
#include "dali/placer/global_placer/rough_legalizer.h"
#include "dali/placer/placer.h"

namespace dali {

class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  void SetMaxIteration(int max_iter);
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);
  void LoadConf(std::string const &config_file) override;

  void InitializeOptimizerAndLegalizer();
  void CloseOptimizerAndLegalizer();

  void InitializeBlockLocation();

  bool StartPlacement() override;

 protected:
  // for look ahead legalization
  int cur_iter_ = 0;
  int max_iter_ = 100;
  double simpl_LAL_converge_criterion_ = 0.005;
  double polar_converge_criterion_ = 0.08;
  int convergence_criteria_ = 1;

  // save intermediate result for debugging and/or visualization
  bool should_save_intermediate_result_ = false;

  bool IsBlockListOrNetListEmpty() const;
  static bool IsSeriesConverge(std::vector<double> &series, int window_size,
                               double tolerance);
  bool IsPlacementConverge();
  void PrintHpwl() const;
  void PrintEndStatement(std::string const &name_of_process,
                         bool is_success) override;

  RandomInitializerType initializer_type_ = RandomInitializerType::UNIFORM;
  HpwlOptimizer *optimizer_ = nullptr;
  RoughLegalizer *legalizer_ = nullptr;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
