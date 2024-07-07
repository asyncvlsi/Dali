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
#include "dali.h"

#include <cstdlib>

#include <iostream>

#include <common/config.h>

#include "dali/common/git_version.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/phydb_helper.h"

namespace dali {

Dali::Dali(
    phydb::PhyDB *phy_db_ptr,
    const std::string &severity_level,
    const std::string &log_file_name
) {
  phy_db_ptr_ = phy_db_ptr;
  severity_level_ = StrToLoggingLevel(severity_level);
  log_file_name_ = log_file_name;
  LoadParamsFromConfig();
  InitLogging(
      log_file_name_,
      severity_level_,
      disable_log_prefix_
  );
}

Dali::Dali(
    phydb::PhyDB *phy_db_ptr,
    severity severity_level,
    const std::string &log_file_name
) {
  phy_db_ptr_ = phy_db_ptr;
  severity_level_ = severity_level;
  log_file_name_ = log_file_name;
  LoadParamsFromConfig();
  InitLogging(
      log_file_name_,
      severity_level_,
      disable_log_prefix_
  );
}

void Dali::LoadParamsFromConfig() {
  std::string param_name = prefix_ + "log_file_name";
  if (config_exists(param_name.c_str())) {
    log_file_name_ = config_get_string(param_name.c_str());
  }

  param_name = prefix_ + "disable_log_prefix";
  if (config_exists(param_name.c_str())) {
    SetLogPrefix(config_get_int(param_name.c_str()) == 1);
  }

  param_name = prefix_ + "num_threads";
  if (config_exists(param_name.c_str())) {
    SetNumThreads(config_get_int(param_name.c_str()));
  }

  param_name = prefix_ + "well_legalization_mode";
  if (config_exists(param_name.c_str())) {
    std::string model_name = config_get_string(param_name.c_str());
    if (model_name == "scavenge") {
      well_legalization_mode_ = DefaultPartitionMode::SCAVENGE;
    } else if (model_name == "strict") {
      well_legalization_mode_ = DefaultPartitionMode::STRICT;
    } else {
      std::cout << "Ignore unknown well_legalization_mode: " << model_name
                << "\n";
    }
  }

  param_name = prefix_ + "disable_global_place";
  if (config_exists(param_name.c_str())) {
    disable_global_place_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "disable_legalization";
  if (config_exists(param_name.c_str())) {
    disable_legalization_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "disable_io_place";
  if (config_exists(param_name.c_str())) {
    disable_io_place_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "target_density";
  if (config_exists(param_name.c_str())) {
    target_density_ = config_get_real(param_name.c_str());
  }

  param_name = prefix_ + "io_metal_layer";
  if (config_exists(param_name.c_str())) {
    io_metal_layer_ = config_get_int(param_name.c_str());
  }

  param_name = prefix_ + "export_well_cluster_matlab";
  if (config_exists(param_name.c_str())) {
    export_well_cluster_matlab_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "disable_welltap";
  if (config_exists(param_name.c_str())) {
    disable_welltap_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "disable_cell_flip";
  if (config_exists(param_name.c_str())) {
    disable_cell_flip_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "max_row_width";
  if (config_exists(param_name.c_str())) {
    max_row_width_ = config_get_real(param_name.c_str());
  }

  param_name = prefix_ + "is_standard_cell";
  if (config_exists(param_name.c_str())) {
    is_standard_cell_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "enable_filler_cell";
  if (config_exists(param_name.c_str())) {
    enable_filler_cell_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "enable_end_cap_cell";
  if (config_exists(param_name.c_str())) {
    enable_end_cap_cell_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "enable_shrink_off_grid_die_area";
  if (config_exists(param_name.c_str())) {
    enable_shrink_off_grid_die_area_ = config_get_int(param_name.c_str()) == 1;
  }

  param_name = prefix_ + "output_name";
  if (config_exists(param_name.c_str())) {
    output_name_ = config_get_string(param_name.c_str());
  }
}

void Dali::SetLogPrefix(bool disable_log_prefix) {
  disable_log_prefix_ = disable_log_prefix;
}

void Dali::SetNumThreads(int num_threads) {
  num_threads_ = num_threads;
}

Circuit &Dali::GetCircuit() {
  return circuit_;
}

phydb::PhyDB *Dali::GetPhyDBPtr() {
  return phy_db_ptr_;
}

bool Dali::ConfigIoPlacerAllInOneLayer(std::string const &layer_name) {
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  bool is_metal_name = circuit_.IsMetalLayerExisting(layer_name);
  if (is_metal_name) {
    MetalLayer *metal_layer = circuit_.GetMetalLayerPtr(layer_name);
    return io_placer_->ConfigSetGlobalMetalLayer(metal_layer->Id());
  }
  return false;
}

bool Dali::ConfigIoPlacer() {
  return true;
}

bool Dali::StartIoPinAutoPlacement() {
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  return io_placer_->AutoPlaceIoPin();
}

void Dali::ReportIoPlacementUsage() {
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Usage: place-io (followed by one of the options below)\n"
    << "  -h/--help\n"
    << "      print out the IO placer usage\n"
    << "  -a/--add <pin_name> <net_name> <direction> <use>\n"
    << "      add an IOPIN\n"
    << "  -p/--place <pin_name> <metal_name> <lx> <ly> <ux> <uy> <x> <y> <orientation>\n"
    << "      manually place an IOPIN\n"
    << "  -c/--config (use -h to see more usage)\n"
    << "      set parameters for automatic IOPIN placement\n"
    << "  -ap/--auto-place\n"
    << "      automatically place all unplaced IOPINs, which is also the default option"
    << "\033[0m\n";
}

bool Dali::IoPinPlacement(int argc, char **argv) {
  if (argc < 2) {
    ReportIoPlacementUsage();
    return false;
  }

  std::string option_str(argv[1]);
  InstantiateIoPlacer();
  if (option_str == "-h" or option_str == "--help") {
    ReportIoPlacementUsage();
    return true;
  }

  // remove "place-io" and option flag before calling each function
  if (option_str == "-c" or option_str == "--config") {
    return io_placer_->ConfigCmd(argc - 2, argv + 2);
  } else if (option_str == "-ap" or option_str == "--auto-place") {
    return io_placer_->AutoPlaceCmd(argc - 2, argv + 2);
  } else {
    BOOST_LOG_TRIVIAL(warning)
      << "IoPlace flag not specified, use --auto-place by default\n";
    return io_placer_->AutoPlaceCmd(argc - 1, argv + 1);
  }
}

bool Dali::ShouldPerformTimingDrivenPlacement() {
  return phy_db_ptr_->GetTimingApi().ReadyForTimingDriven();
}

void Dali::InitializeRCEstimator() {
  rc_estimator = new StarPiModelEstimator(phy_db_ptr_);
}

#if PHYDB_USE_GALOIS
void Dali::FetchSlacks() {
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
  std::cout << "Number of timing constraints: "
            << timing_api.GetNumConstraints() << "\n";
  for (int i = 0; i < (int) timing_api.GetNumConstraints(); ++i) {
    double slack = timing_api.GetSlack(i);
    std::cout << "Slack for timing constraint " << i << " " << slack << "\n";
    if (slack < 0) {
      phydb::PhydbPath fast_path;
      timing_api.GetFastWitness(i, fast_path);
      std::cout << "Fast path size: " << fast_path.edges.size() << "\n";
      phydb::PhydbPath slow_path;
      timing_api.GetSlowWitness(i, fast_path);
      std::cout << "Fast path size: " << slow_path.edges.size() << "\n";
    }
  }
}

void Dali::InitializeTimingDrivenPlacement() {
  phy_db_ptr_->CreatePhydbActAdaptor();
  phy_db_ptr_->AddNetsAndCompPinsToSpefManager();
  InitializeRCEstimator();
}

void Dali::UpdateRCs() {
  rc_estimator->PushNetRCToManager();
}

void Dali::PerformTimingAnalysis() {
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
  timing_api.UpdateTimingIncremental();
}

void Dali::UpdateNetWeights() {
  FetchSlacks();
}

void Dali::ReportPerformance() {
  if (!phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) return;
}

bool Dali::TimingDrivenPlacement(double density, int number_of_threads) {
  bool is_success = true;
  InitializeTimingDrivenPlacement();
  for (int i = 0; i < max_td_place_num_; ++i) {
    GlobalPlace(density, number_of_threads);
    is_success = UnifiedLegalization();
    UpdateRCs();
    PerformTimingAnalysis();
    UpdateNetWeights();
  }
  ReportPerformance();
  return is_success;
}

#endif

bool Dali::StartPlacement(double density, int number_of_threads) {
  if (density > 0) {
    target_density_ = density;
  }
  if (number_of_threads >= 1) {
    num_threads_ = number_of_threads;
  }

  circuit_.SetEnableShrinkOffGridDieArea(enable_shrink_off_grid_die_area_);
  circuit_.InitializeFromPhyDB(phy_db_ptr_);
  circuit_.ReportBriefSummary();

  // set the placement density
  if (target_density_ == -1) {
    double default_density = 0.7;
    target_density_ = std::max(circuit_.WhiteSpaceUsage(), default_density);
    BOOST_LOG_TRIVIAL(info)
      << "Target density not provided, set it to default value: "
      << target_density_ << "\n";
  }

  // start placement
  // (1). global placement
  gb_placer_.SetInputCircuit(&circuit_);
  gb_placer_.SetNumThreads(num_threads_);
  if (!disable_global_place_) {
    gb_placer_.SetPlacementDensity(target_density_);
    //gb_placer->ReportBoundaries();
    gb_placer_.StartPlacement();
  }
  if (export_well_cluster_matlab_) {
    circuit_.GenMATLABTable("gb_result.txt");
  }

  // (2). legalization
  if (!disable_legalization_) {
    if (is_standard_cell_) {
      legalizer_.TakeOver(&gb_placer_);
      legalizer_.disable_cell_flip_ = disable_cell_flip_;
      legalizer_.StartPlacement();
    } else {
      well_legalizer_.TakeOver(&gb_placer_);
      well_legalizer_.disable_welltap_ = disable_welltap_;
      well_legalizer_.disable_cell_flip_ = disable_cell_flip_;
      well_legalizer_.enable_end_cap_cell_ = enable_end_cap_cell_;
      well_legalizer_.SetStripePartitionMode(static_cast<int>(well_legalization_mode_));
      well_legalizer_.StartPlacement();
      if (export_well_cluster_matlab_) {
        well_legalizer_.GenMatlabClusterTable("sc_result");
        well_legalizer_.GenMATLABWellTable("scw", 0);
      }
      well_legalizer_.EmitDEFWellFile("dali_out", 1);
    }
  }
  if (export_well_cluster_matlab_) {
    circuit_.GenMATLABTable("lg_result.txt");
  }

  if (enable_filler_cell_) {
    filler_cell_placer_.TakeOver(&gb_placer_);
    filler_cell_placer_.phy_db_ptr_ = phy_db_ptr_;
    filler_cell_placer_.CreateFillerCellTypes(2);
    filler_cell_placer_.StartPlacement();
  }

  if (!disable_io_place_) {
    auto io_placer = std::make_unique<IoPlacer>(phy_db_ptr_, &circuit_);
    bool is_ioplacer_config_success =
        io_placer->ConfigSetGlobalMetalLayer(io_metal_layer_);
    DaliExpects(
        is_ioplacer_config_success,
        "Cannot successfully configure I/O placer"
    );
    io_placer->AutoPlaceIoPin();
  }

  BOOST_LOG_TRIVIAL(debug) << "dali git commit: " << get_git_version_short() << "\n";

  return true;
}

void Dali::AddWellTaps(
    phydb::Macro *cell,
    double cell_interval_microns,
    bool is_checker_board
) {
  well_tap_placer_ = new WellTapPlacer(phy_db_ptr_);

  well_tap_placer_->FetchRowsFromPhyDB();
  well_tap_placer_->InitializeWhiteSpaceInRows();

  well_tap_placer_->SetCell(cell);
  well_tap_placer_->SetCellInterval(cell_interval_microns);
  well_tap_placer_->SetCellMinDistanceToBoundary(0.1);
  well_tap_placer_->UseCheckerBoardMode(is_checker_board);

  well_tap_placer_->AddWellTap();
  well_tap_placer_->PlotAvailSpace();

  well_tap_placer_->ExportWellTapCellsToPhyDB();

  delete well_tap_placer_;
}

bool Dali::AddWellTaps(int argc, char **argv) {
  phydb::Macro *cell = nullptr;
  double cell_interval_microns = -1;
  bool is_checker_board = false;
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-cell" && i < argc) {
      std::string macro_name = std::string(argv[i++]);
      cell = phy_db_ptr_->GetMacroPtr("WELLTAPX1");
      if (cell == nullptr) {
        std::cout << "Cannot find well-tap cell: " << macro_name << "\n";
        return false;
      }
      if (cell->GetClass() != phydb::MacroClass::CORE_WELLTAP) {
        std::cout << "Given cell is not well-tap cell\n";
        return false;
      }
    } else if (arg == "-interval" && i < argc) {
      std::string cell_interval_str = std::string(argv[i++]);
      try {
        cell_interval_microns = std::stod(cell_interval_str);
      } catch (...) {
        std::cout << "Invalid well-tap cell interval\n";
        return false;
      }
    } else if (arg == "-checker_board") {
      is_checker_board = true;
    } else {
      std::cout << "Unknown flag\n";
      std::cout << arg << "\n";
      return false;
    }
  }

  AddWellTaps(cell, cell_interval_microns, is_checker_board);
  return true;
}

/**
 * Perform global placement using a given target density.
 *
 * @param density: target density, reasonable range (0, 1].
 * @param num_threads: number of threads.
 * @return void
 */
bool Dali::GlobalPlace(double density, int num_threads) {
  gb_placer_.SetNumThreads(num_threads);
  gb_placer_.SetInputCircuit(&circuit_);
  gb_placer_.SetShouldSaveIntermediateResult(false);
  gb_placer_.SetBoundaryFromCircuit();
  gb_placer_.SetPlacementDensity(density);
  return gb_placer_.StartPlacement();
}

/**
 * Perform unified legalization for a gridded cell design.
 * Unified legalization includes removing cell overlaps and
 * fixing all N/P well design rule violations.
 *
 * N/P well rectangles, NP/PP rectangles, and clusters will
 * be generated in this step.
 *
 * @param density, target density, reasonable range (0, 1].
 * @return void
 */
bool Dali::UnifiedLegalization() {
  well_legalizer_.TakeOver(&gb_placer_);
  well_legalizer_.SetStripePartitionMode(int(DefaultPartitionMode::SCAVENGE));
  well_legalizer_.is_dump = false;
  return well_legalizer_.StartPlacement();
}

/**
 * Perform detailed placement using external placers.
 *
 * @param engine, the full path or name of external placer. If name is given,
 *        this placer is required to be in the search path.
 * @param load_dp_result, a boolean variable, if true, then load detailed
 *        placement back to PhyDB database.
 * @return void
 */
void Dali::ExternalDetailedPlaceAndLegalize(
    std::string const &engine,
    bool load_dp_result
) {
  // create a script for detailed placement and legalization
  BOOST_LOG_TRIVIAL(info)
    << "Creating detailed placement and legalization script...\n";
  std::string dp_script_name = "dali_" + engine + ".cmd";
  std::string legal_def_file =
      CreateDetailedPlacementAndLegalizationScript(engine, dp_script_name);

  // system call
  BOOST_LOG_TRIVIAL(info) << "System call...\n";
  std::string command = engine + " < " + dp_script_name;
  int res = std::system(command.c_str());
  BOOST_LOG_TRIVIAL(info) << engine << " return code: " << res << "\n";

  if (load_dp_result) {
    phy_db_ptr_->OverrideComponentLocsFromDef(legal_def_file);
    BOOST_LOG_TRIVIAL(info) << "New placement loaded back to PhyDB\n";
  }
}

void Dali::ExportToPhyDB() {
  // 1. COMPONENTS
  ExportComponentsToPhyDB();
  // 2. IOPINs
  ExportIoPinsToPhyDB();
  if (well_legalizer_.ckt_ptr_ != nullptr) {
    // 3. MiniRows
    ExportMiniRowsToPhyDB();
    // 4. NP/PP and Well
    ExportPpNpToPhyDB();
    ExportWellToPhyDB();
  }
}

void Dali::Close() {
  CloseLogging();
}

void Dali::ExportToDEF(
    std::string const &input_def_file_full_name,
    std::string const &output_def_name
) {
  circuit_.SaveDefFile(
      output_def_name,
      "",
      input_def_file_full_name,
      1,
      1,
      2,
      1
  );
  circuit_.SaveDefFile(
      output_def_name,
      "_io",
      input_def_file_full_name,
      1,
      1,
      1,
      1
  );
  circuit_.SaveDefFile(
      output_def_name,
      "_filling",
      input_def_file_full_name,
      1,
      4,
      2,
      0
  );
  circuit_.SaveDefFileComponent(
      output_def_name + "_comp.def",
      input_def_file_full_name
  );
  circuit_.InitNetFanoutHistogram();
  circuit_.ReportNetFanoutHistogram();
  circuit_.ReportHPWLHistogramLinear();
  circuit_.ReportHPWLHistogramLogarithm();
}

void Dali::InstantiateIoPlacer() {
  if (io_placer_ == nullptr) {
    io_placer_ = new IoPlacer(phy_db_ptr_, &circuit_);
  }
}

/**
 * Create a script for detailed placement and legalization.
 *
 * @param engine, the full path or name of an external placer. If name is given,
 *        this placer is required to be in the search path.
 * @param script_name, the name of this detailed placement and legalization script.
 * @return the DEF file name generated by the external placer.
 */
std::string Dali::CreateDetailedPlacementAndLegalizationScript(
    std::string const &engine,
    std::string const &script_name
) {
  // create script for innovus
  DaliExpects(engine == "innovus", "Only support Innovus now");
  std::ofstream ost(script_name);
  ost << "loadLefFile " << phy_db_ptr_->GetTechPtr()->GetLefName() << "\n";
  std::string input_def_file = phy_db_ptr_->GetDesignPtr()->GetDefName();
  std::string
      tmp_def_out = "dali_global_" + phy_db_ptr_->GetDesignPtr()->GetName();
  ExportToDEF(input_def_file, tmp_def_out);
  ost << "loadDefFile " << tmp_def_out << ".def\n";
  ost << "refinePlace\n";
  std::string out_def =
      "dali_global_innovus_refine_" + phy_db_ptr_->GetDesignPtr()->GetName()
          + ".def";
  ost << "defOut " << out_def << "\n";

  // check if the engine can be found
  DaliExpects(IsExecutableExisting(engine), "Cannot find the given engine");
  return out_def;
}

void Dali::ExportOrdinaryComponentsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &block : circuit_.Blocks()) {
    if (block.TypePtr() == circuit_.tech().IoDummyBlkTypePtr()) {
      // skip dummy cells for I/O pins
      continue;
    }
    std::string comp_name = block.Name();
    int lx = (int) (block.LLX() * factor_x)
        + circuit_.design().DieAreaOffsetX();
    int ly = (int) (block.LLY() * factor_y)
        + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(block.Status());
    auto orient = phydb::CompOrient(block.Orient());

    phydb::Component *comp_ptr = phy_db_ptr_->GetComponentPtr(comp_name);
    DaliExpects(comp_ptr != nullptr,
                "No component in PhyDB with name: " << comp_name);
    comp_ptr->SetLocation(lx, ly);
    comp_ptr->SetPlacementStatus(place_status);
    comp_ptr->SetOrientation(orient);
  }
}

void Dali::ExportWellTapCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &block : circuit_.design().WellTaps()) {
    std::string comp_name = block.Name();
    std::string macro_name = block.TypePtr()->Name();
    int lx = (int) (block.LLX() * factor_x)
        + circuit_.design().DieAreaOffsetX();
    int ly = (int) (block.LLY() * factor_y)
        + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(block.Status());
    auto orient = phydb::CompOrient(block.Orient());

    auto *phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(
        phydb_macro_ptr != nullptr,
        "Cannot find " << macro_name << " in PhyDB?!"
    );
    phy_db_ptr_->AddComponent(
        comp_name,
        phydb_macro_ptr,
        place_status,
        lx, ly,
        orient,
        phydb::CompSource::USER
    );
  }
}

void Dali::ExportFillerCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &block : circuit_.design().Fillers()) {
    std::string comp_name = block.Name();
    std::string macro_name = block.TypePtr()->Name();
    int lx = (int) (block.LLX() * factor_x)
        + circuit_.design().DieAreaOffsetX();
    int ly = (int) (block.LLY() * factor_y)
        + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(block.Status());
    auto orient = phydb::CompOrient(block.Orient());

    auto *phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(
        phydb_macro_ptr != nullptr,
        "Cannot find " << macro_name << " in PhyDB?!"
    );
    phy_db_ptr_->AddComponent(
        comp_name,
        phydb_macro_ptr,
        place_status,
        lx, ly,
        orient,
        phydb::CompSource::DIST
    );
  }
}

void Dali::ExportComponentsToPhyDB() {
  ExportOrdinaryComponentsToPhyDB();
  ExportWellTapCellsToPhyDB();
  ExportFillerCellsToPhyDB();
}

void Dali::ExportIoPinsToPhyDB() {
  DaliExpects(
      !circuit_.Metals().empty(),
      "Need metal layer info to generate PIN location\n"
  );
  for (auto &iopin : circuit_.design().IoPins()) {
    if (!iopin.IsPrePlaced() && iopin.IsPlaced()) {
      DaliExpects(
          iopin.LayerPtr() != nullptr,
          "IOPIN metal layer not set? Cannot export it to PhyDB"
      );
      std::string metal_name = iopin.LayerPtr()->Name();
      std::string iopin_name = iopin.Name();
      DaliExpects(
          phy_db_ptr_->IsIoPinExisting(iopin_name),
          "IOPIN not in PhyDB? " << iopin_name
      );
      phydb::IOPin *phydb_iopin = phy_db_ptr_->GetIoPinPtr(iopin_name);
      auto &rect = iopin.GetShape();
      int llx = circuit_.Micron2DatabaseUnit(rect.LLX());
      int lly = circuit_.Micron2DatabaseUnit(rect.LLY());
      int urx = circuit_.Micron2DatabaseUnit(rect.URX());
      int ury = circuit_.Micron2DatabaseUnit(rect.URY());
      phydb_iopin->SetShape(metal_name, llx, lly, urx, ury);

      int pin_x = iopin.FinalX();
      int pin_y = iopin.FinalY();
      phydb::CompOrient pin_orient;
      if (iopin.X() == circuit_.design().RegionLeft()) {
        pin_orient = phydb::CompOrient::E;
      } else if (iopin.X() == circuit_.design().RegionRight()) {
        pin_orient = phydb::CompOrient::W;
      } else if (iopin.Y() == circuit_.design().RegionBottom()) {
        pin_orient = phydb::CompOrient::N;
      } else {
        pin_orient = phydb::CompOrient::S;
      }

      phydb_iopin->SetPlacement(
          PlaceStatusDali2PhyDB(iopin.GetPlaceStatus()), pin_x,
          pin_y, pin_orient
      );
    }
  }
}

void Dali::ExportMiniRowsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();

  int counter = 0;
  for (auto &col : well_legalizer_.col_list_) {
    for (auto &strip : col.stripe_list_) {
      std::string column_name = "column" + std::to_string(counter++);
      std::string bot_signal_;
      if (strip.is_first_row_orient_N_) {
        bot_signal_ = "GND";
      } else {
        bot_signal_ = "Vdd";
      }
      phydb::ClusterCol *p_col =
          phy_db_ptr_->AddClusterCol(column_name, bot_signal_);

      int col_lx = (int) (strip.LLX() * factor_x)
          + circuit_.design().DieAreaOffsetX();
      int col_ux = (int) (strip.URX() * factor_x)
          + circuit_.design().DieAreaOffsetX();
      p_col->SetXRange(col_lx, col_ux);

      if (strip.is_bottom_up_) {
        for (auto &cluster : strip.gridded_rows_) {
          int row_ly = (int) (cluster.LLY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          int row_uy = (int) (cluster.URY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      } else {
        int sz = static_cast<int>(strip.gridded_rows_.size());
        for (int j = sz - 1; j >= 0; --j) {
          auto &cluster = strip.gridded_rows_[j];
          int row_ly = (int) (cluster.LLY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          int row_uy = (int) (cluster.URY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      }
    }
  }
}

void Dali::ExportPpNpToPhyDB() {
  well_legalizer_.ExportPpNpToPhyDB(phy_db_ptr_);
}

void Dali::ExportWellToPhyDB() {
  well_legalizer_.ExportWellToPhyDB(phy_db_ptr_, 1);
}

}
