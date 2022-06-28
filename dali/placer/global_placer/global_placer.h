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
#include <cfloat>

#include <queue>
#include <random>
#include <set>
#include <map>
#include <vector>

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

#include "blkpairnets.h"
#include "dali/common/misc.h"
#include "dali/placer/placer.h"

#include "box_bin.h"
#include "cell_cut_point.h"
#include "grid_bin_index.h"
#include "grid_bin.h"

#include "hpwl_optimizer.h"
#include "rough_legalizer.h"

namespace dali {

class GlobalPlacer : public Placer {
 public:
  GlobalPlacer() = default;

  void SetMaxIteration(int max_iter);
  void SetShouldSaveIntermediateResult(bool should_save_intermediate_result);
  void LoadConf(std::string const &config_file) override;

  void InitializeOptimizerAndLegalizer();
  void CloseOptimizerAndLegalizer();

  void SetBlockLocationInitialization(int mode, double std_dev);
  void InitializeBlockLocationAtRandom();

  bool StartPlacement() override;
 protected:
  // stop update net model if the cost change is less than this value for 3 iterations
  double net_model_update_stop_criterion_ = 0.01;

  // for look ahead legalization
  int cur_iter_ = 0;
  int max_iter_ = 100;
  double simpl_LAL_converge_criterion_ = 0.005;
  double polar_converge_criterion_ = 0.08;
  int convergence_criteria_ = 1;

  // save intermediate result for debugging and/or visualization
  bool should_save_intermediate_result_ = false;

  // block location initialization
  int mode_ = 0;
  double std_dev_ = -1;
  void BlockLocationUniformInitialization();
  void BlockLocationNormalInitialization(double std_dev);

  bool IsBlockListOrNetListEmpty() const;
  static bool IsSeriesConverge(
      std::vector<double> &data,
      int window_size,
      double tolerance
  );
  bool IsPlacementConverge();
  void PrintHpwl(int iter, double lo, double hi) const;
  void PrintPlacementSummary() const;

  void DumpResult(std::string const &name_of_file);

  HpwlOptimizer *optimizer_ = nullptr;
  RoughLegalizer *legalizer_ = nullptr;
};

}

#endif //DALI_PLACER_GLOBAL_PLACER_GLOBAL_PLACER_H_
