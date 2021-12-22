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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_

#include "dali/placer/placer.h"
#include "spacepartitioner.h"
#include "stripe.h"

namespace dali {

class GriddedRowLegalizer : public Placer {
 public:
  GriddedRowLegalizer() = default;

  void CheckWellInfo();

  void SetExternalSpacePartitioner(AbstractSpacePartitioner *p_external_partitioner);
  void SetPartitionMode(int partitioning_mode_);
  void PartitionSpaceAndBlocks();

  void SetWellTapCellParameters(
      double tap_cell_interval_microns,
      bool is_checker_board_mode = false,
      std::string const &well_tap_type_name = ""
  );

  void SetLegalizationMaxIteration(size_t max_iteration);
  bool StripeLegalizationUpward(Stripe &stripe) const;
  bool StripeLegalizationDownward(Stripe &stripe) const;
  bool GroupBlocksToClusters();

  void StretchBlocks();

  void EmbodyWellTapCells();

  bool StartPlacement() override;
  void GenMatlabClusterTable(std::string const &name_of_file);
  void GenMATLABWellTable(
      std::string const &name_of_file,
      int well_emit_mode
  ) override;
 private:
  // space partitioner
  int partitioning_mode_ = 0;
  AbstractSpacePartitioner *space_partitioner_ = nullptr;

  int well_spacing_ = 0;
  int tap_cell_p_height_ = 0;
  int tap_cell_n_height_ = 0;

  std::vector<ClusterStripe> col_list_;

  int tap_cell_interval_grid_ = -1;
  bool is_checker_board_mode_ = false;
  BlockType *well_tap_type_ptr_ = nullptr;

  size_t max_iteration_ = 10;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_GRIDDEDROWLEGALIZER_H_
