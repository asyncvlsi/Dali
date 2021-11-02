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
#ifndef DALI_DALI_DALI_H_
#define DALI_DALI_DALI_H_

#include <phydb/phydb.h>

#include "circuit/circuit.h"
#include "placer.h"
#include "timing/starpimodelestimator.h"

namespace dali {

class Dali {
 public:
  Dali(
      phydb::PhyDB *phy_db_ptr,
      const std::string &severity_level,
      const std::string &log_file_name = "",
      bool is_log_no_prefix = false
  );
  Dali(
      phydb::PhyDB *phy_db_ptr,
      severity severity_level,
      const std::string &log_file_name = "",
      bool is_log_no_prefix = false
  );

  bool AddIoPin();
  bool SetIoPinStatus();
  bool ConfigIoPlacerAllInOneLayer(std::string const &layer_name);
  bool ConfigIoPlacer();
  bool StartIoPinAutoPlacement();
  bool IoPinPlacement(int argc, char **argv);

  void InitializeRCEstimator();
  void FetchSlacks();

  void StartPlacement(double density, int number_of_threads = 1);

  void AddWellTaps(
      phydb::Macro *cell,
      double cell_interval_microns,
      bool is_checker_board
  );
  bool AddWellTaps(int argc, char **argv);
  void GlobalPlace(double density);
  void UnifiedLegalization();

  void ExternalDetailedPlaceAndLegalize(
      std::string const &engine,
      bool load_dp_result = true
  );

  void ExportToPhyDB();
  void Close();

  void ExportToDEF(
      std::string const &input_def_file_full_name,
      std::string const &output_def_name = "circuit"
  );

  void InstantiateIoPlacer();
 private:
  Circuit circuit_;
  phydb::PhyDB *phy_db_ptr_ = nullptr;
  GPSimPL gb_placer_;
  LGTetrisEx legalizer_;
  StdClusterWellLegalizer well_legalizer_;
  WellTapPlacer *well_tap_placer_ = nullptr;
  IoPlacer *io_placer_ = nullptr;
  StarPiModelEstimator *rc_estimator = nullptr;

  static void ReportIoPlacementUsage();

  std::string CreateDetailedPlacementAndLegalizationScript(
      std::string const &engine,
      std::string const &script_name
  );

  void ExportOrdinaryComponentsToPhyDB();
  void ExportWellTapCellsToPhyDB();
  void ExportComponentsToPhyDB();
  void ExportIoPinsToPhyDB();
  void ExportMiniRowsToPhyDB();
  void ExportPpNpToPhyDB();
  void ExportWellToPhyDB();
};

}

#endif //DALI_DALI_DALI_H_
