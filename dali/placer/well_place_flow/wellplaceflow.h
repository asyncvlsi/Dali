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

/** End-to-end placement flow with global placement and well legalization. */
class WellPlaceFlow : public GlobalPlacer {
  StdClusterWellLegalizer well_legalizer_;

 public:
  WellPlaceFlow();

  /** Run global placement followed by gridded well legalization. */
  bool StartPlacement() override;

  /** Emit DEF with well-legalized cells and optional cluster geometry. */
  void EmitDEFWellFile(std::string const& name_of_file, int well_emit_mode,
                       bool enable_emitting_cluster = true) override;
};

}  // namespace dali

#endif  // DALI_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
