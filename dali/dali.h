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
#ifndef DALI_DALI_H_
#define DALI_DALI_H_

#include <phydb/phydb.h>

#include "dali/circuit/circuit.h"
#include "dali/placer.h"
#include "dali/timing/starpimodelrcestimator.h"

namespace dali {

class Dali {
 public:
  Dali(
      phydb::PhyDB *phy_db_ptr,
      const std::string &severity_level,
      const std::string &log_file_name = ""
  );
  Dali(
      phydb::PhyDB *phy_db_ptr,
      severity severity_level,
      const std::string &log_file_name = ""
  );

  void ShowParamsList();
  void LoadParamsFromConfig();

  void SetLogPrefix(bool disable_log_prefix);
  void SetNumThreads(int num_threads);

  Circuit &GetCircuit();
  phydb::PhyDB *GetPhyDBPtr();

  bool ConfigIoPlacerAllInOneLayer(std::string const &layer_name);
  bool ConfigIoPlacer();
  bool StartIoPinAutoPlacement();
  bool IoPinPlacement(int argc, char **argv);

  bool ShouldPerformTimingDrivenPlacement();
  void InitializeRCEstimator();
#if PHYDB_USE_GALOIS
  void FetchSlacks();
  void InitializeTimingDrivenPlacement();
  void UpdateRCs();
  void PerformTimingAnalysis();
  void UpdateNetWeights();
  void ReportPerformance();
  bool TimingDrivenPlacement(double density, int number_of_threads);
#endif

  void CreateFillerCells(int upper_width);
  bool StartPlacement(double density = -1, int number_of_threads = 1);

  void AddWellTaps(
      phydb::Macro *cell,
      double cell_interval_microns,
      bool is_checker_board
  );
  bool AddWellTaps(int argc, char **argv);
  bool GlobalPlace(double density, int num_threads = 1);
  bool UnifiedLegalization();

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
  // options
  std::string prefix_ = "dali.";
  severity severity_level_ = boost::log::trivial::info;
  std::string log_file_name_;
  bool disable_log_prefix_ = false;
  int num_threads_ = 1;
  DefaultPartitionMode well_legalization_mode_ = DefaultPartitionMode::STRICT;
  bool disable_global_place_ = false;
  bool disable_legalization_ = false;
  bool disable_io_place_ = false;
  double target_density_ = -1;
  int io_metal_layer_ = 0;
  bool export_well_cluster_matlab_ = false;
  bool disable_welltap_ = true;
  double max_row_width_ = 0;
  bool is_standard_cell_ = false;
  bool enable_filler_cell_ = false;
  std::string output_name_ = "dali_out";

  // circuit and placer
  Circuit circuit_;
  phydb::PhyDB *phy_db_ptr_ = nullptr;
  GlobalPlacer gb_placer_;
  LGTetrisEx legalizer_;
  StdClusterWellLegalizer well_legalizer_;
  WellTapPlacer *well_tap_placer_ = nullptr;
  FillerCellPlacer filler_cell_placer_;
  IoPlacer *io_placer_ = nullptr;
  StarPiModelEstimator *rc_estimator = nullptr;

  int max_td_place_num_ = 2;

  static void ReportIoPlacementUsage();

  std::string CreateDetailedPlacementAndLegalizationScript(
      std::string const &engine,
      std::string const &script_name
  );

  void ExportOrdinaryComponentsToPhyDB();
  void ExportWellTapCellsToPhyDB();
  void ExportFillerCellsToPhyDB();
  void ExportComponentsToPhyDB();
  void ExportIoPinsToPhyDB();
  void ExportMiniRowsToPhyDB();
  void ExportPpNpToPhyDB();
  void ExportWellToPhyDB();
};

}

#endif //DALI_DALI_H_
