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
#ifndef DALI_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
#define DALI_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_

#include "dali/placer/global_placer/global_placer.h"
#include "dali/placer/legalizer/LGTetrisEx.h"
#include "dali/placer/well_legalizer/stdclusterwelllegalizer.h"

namespace dali {

class WellPlaceFlow : public GlobalPlacer {
  StdClusterWellLegalizer well_legalizer_;
 public:
  WellPlaceFlow();

  void SetGridCapacity(int gc) {
    number_of_cell_in_bin_ = gc;
    BOOST_LOG_TRIVIAL(info) << "Bin area: " << number_of_cell_in_bin_
                            << " average cell area\n";
  }
  void SetIteration(int it_num) {
    cg_iteration_ = it_num;
    BOOST_LOG_TRIVIAL(info) << "Max number of iteration: " << cg_iteration_
                            << " \n";
  }
  bool StartPlacement() override;

  void EmitDEFWellFile(
      std::string const &name_of_file,
      int well_emit_mode,
      bool enable_emitting_cluster = true
  ) override;
};

}

#endif //DALI_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
