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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_METAROWLEGALIZER_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_METAROWLEGALIZER_H_

#include "dali/placer/placer.h"
#include "spacepartitioner.h"
#include "stripe.h"

namespace dali {

class MetaRowLegalizer : public Placer {
 public:
  MetaRowLegalizer() = default;

  void CheckWellInfo();

  void SetPartitionMode(StripePartitionMode mode);
  void PartitionSpaceAndBlocks();

  bool StripeLegalizationBottomUp(Stripe &stripe);
  bool GroupBlocksToClusters();

  void StretchBlocks();

  bool StartPlacement() override;

  void GenMatlabClusterTable(std::string const &name_of_file);
 private:
  StripePartitionMode stripe_mode_ = StripePartitionMode::STRICT;
  SpacePartitioner space_partitioner_;

  int well_spacing_ = 0;
  int tap_cell_p_height_;
  int tap_cell_n_height_;

  std::vector<ClusterStripe> col_list_;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_METAROWLEGALIZER_H_
