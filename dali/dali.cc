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

#include "dali/common/helper.h"
#include "dali/common/phydbhelper.h"

namespace dali {

Dali::Dali(
    phydb::PhyDB *phy_db_ptr,
    const std::string &severity_level,
    const std::string &log_file_name,
    bool is_log_no_prefix
) {
  auto boost_severity_level = StrToLoggingLevel(severity_level);
  InitLogging(
      log_file_name,
      false,
      boost_severity_level,
      is_log_no_prefix
  );
  phy_db_ptr_ = phy_db_ptr;
  circuit_.InitializeFromPhyDB(phy_db_ptr);
}

Dali::Dali(
    phydb::PhyDB *phy_db_ptr,
    severity severity_level,
    const std::string &log_file_name,
    bool is_log_no_prefix
) {
  InitLogging(
      log_file_name,
      false,
      severity_level,
      is_log_no_prefix
  );
  phy_db_ptr_ = phy_db_ptr;
  circuit_.InitializeFromPhyDB(phy_db_ptr);
}

void Dali::InstantiateIoPlacer() {
  if (io_placer_ == nullptr) {
    io_placer_ = new IoPlacer(phy_db_ptr_, &circuit_);
  }
}

bool Dali::AddIoPin(
    std::string &iopin_name,
    std::string &net_name,
    phydb::SignalDirection direction,
    phydb::SignalUse use
) {
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  return io_placer_->AddIoPin(iopin_name, net_name, direction, use);
}

bool Dali::PlaceIoPin(
    std::string &iopin_name,
    std::string &metal_name,
    int shape_lx,
    int shape_ly,
    int shape_ux,
    int shape_uy,
    phydb::PlaceStatus place_status,
    int loc_x,
    int loc_y,
    phydb::CompOrient orient
) {
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  return io_placer_->PlaceIoPin(iopin_name, metal_name,
                                shape_lx, shape_ly, shape_ux, shape_uy,
                                place_status, loc_x, loc_y, orient);
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
  if (option_str == "-a" or option_str == "--add") {
    return io_placer_->AddCmd(argc - 2, argv + 2);
  } else if (option_str == "-p" or option_str == "--place") {
    return io_placer_->PlaceCmd(argc - 2, argv + 2);
  } else if (option_str == "-c" or option_str == "--config") {
    return io_placer_->ConfigCmd(argc - 2, argv + 2);
  } else if (option_str == "-ap" or option_str == "--auto-place") {
    return io_placer_->AutoPlaceCmd(argc - 2, argv + 2);
  } else {
    BOOST_LOG_TRIVIAL(warning)
      << "IoPlace flag not specified, use --auto-place by default\n";
    return io_placer_->AutoPlaceCmd(argc - 1, argv + 1);
  }
}

void Dali::InitializeRCEstimator() {
  rc_estimator = new StarPiModelEstimator(phy_db_ptr_, &circuit_);
}

void Dali::FetchSlacks() {
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
  std::cout << "Number of timing constraints: "
            << timing_api.GetNumConstraints() << "\n";
  for (int i = 0; i < (int) timing_api.GetNumConstraints(); ++i) {
    double slack = timing_api.GetSlack(i);
    std::cout << "Slack for timing constraint " << i << " " << slack
              << "\n";
    if (slack < 0) {
      phydb::PhydbPath fast_path;
      phydb::PhydbPath slow_path;
      timing_api.GetWitness(i, fast_path, slow_path);
      std::cout << "Fast path size: "
                << fast_path.edges.size() << "\n";
      std::cout << "Fast path size: "
                << slow_path.edges.size() << "\n";
    }
  }
}

void Dali::StartPlacement(double density, int number_of_threads) {
  int num_of_thread_openmp = number_of_threads;
  omp_set_num_threads(num_of_thread_openmp);
  Eigen::initParallel();
  BOOST_LOG_TRIVIAL(info) << "Eigen thread " << Eigen::nbThreads() << "\n";

  BOOST_LOG_TRIVIAL(info) << "  Average white space utility: "
                          << circuit_.WhiteSpaceUsage() << std::endl;
  circuit_.ReportBriefSummary();
  //circuit.ReportBlockType();
  //circuit.ReportIOPin();
  circuit_.ReportHPWL();
  //circuit.BuildBlkPairNets();

  GlobalPlace(density);

#if PHYDB_USE_GALOIS
  if (phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) {
    BOOST_LOG_TRIVIAL(debug) << "Before CreatePhydbActAdaptor()\n";
    phy_db_ptr_->CreatePhydbActAdaptor();
    BOOST_LOG_TRIVIAL(debug)
      << "Before AddNetsAndCompPinsToSpefManager()\n";
    phy_db_ptr_->AddNetsAndCompPinsToSpefManager();
    //FetchSlacks();
    BOOST_LOG_TRIVIAL(debug) << "Before InitializeRCEstimator()\n";
    InitializeRCEstimator();
    BOOST_LOG_TRIVIAL(debug) << "Before PushNetRCToManager()\n";
    rc_estimator->PushNetRCToManager();
    phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
    BOOST_LOG_TRIVIAL(debug) << "Before UpdateTimingIncremental()\n";
    timing_api.UpdateTimingIncremental();
    BOOST_LOG_TRIVIAL(debug) << "Before GetNumConstraints()\n";
    FetchSlacks();
  }
#endif
  UnifiedLegalization();
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
        std::cout << "Cannot find well-tap cell: " << macro_name
                  << "\n";
        return false;
      }
      if (cell->GetClass() != phydb::CORE_WELLTAP) {
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
 * @param density, target density, reasonable range (0, 1].
 * @return void
 */
void Dali::GlobalPlace(double density) {
  //std::string config_file = "dali.conf";

  //gb_placer_.LoadConf(config_file);
  gb_placer_.SetInputCircuit(&circuit_);
  gb_placer_.is_dump = false;
  gb_placer_.SetBoundaryDef();
  gb_placer_.SetFillingRate(density);
  gb_placer_.ReportBoundaries();
  gb_placer_.StartPlacement();
  //gb_placer_.SaveDEFFile("benchmark_1K_dali.def", def_file_name);
  //gb_placer_.GenMATLABTable("gb_result.txt");
  //circuit.GenLongNetTable("gb_longnet.txt");
  //gb_placer_->GenMATLABWellTable("gb_result");
  //circuit.ReportNetFanoutHisto();
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
void Dali::UnifiedLegalization() {
  /*
  legalizer_.TakeOver(&gb_placer_);
  legalizer_.IsPrintDisplacement(true);
  legalizer_.is_dump = false;
  legalizer_.StartPlacement();
  legalizer_.GenMATLABTable("lg_result.txt");
  legalizer_.GenDisplacement("disp_result.txt");
  //circuit.GenLongNetTable("lg_longnet.txt");
  //legalizer_->SaveDEFFile("circuit.def", def_file);
   */

  well_legalizer_.TakeOver(&gb_placer_);
  well_legalizer_.SetStripePartitionMode(SCAVENGE);
  well_legalizer_.is_dump = false;
  well_legalizer_.StartPlacement();
  //well_legalizer_.GenMatlabClusterTable("sc_result");
  //well_legalizer_.GenMATLABTable("sc_result.txt");
  //well_legalizer_.GenMatlabClusterTable("sc_result");
  //well_legalizer_.GenMATLABWellTable("scw", 0);
  //circuit.GenLongNetTable("sc_longnet.txt");

  //well_legalizer_.EmitDEFWellFile("circuit", 1, false);
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
  if (well_legalizer_.p_ckt_ != nullptr) {
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
      1
  );
  circuit_.InitNetFanoutHistogram();
  circuit_.ReportNetFanoutHistogram();
  circuit_.ReportHPWLHistogramLinear();
  circuit_.ReportHPWLHistogramLogarithm();
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
  for (auto &block: circuit_.Blocks()) {
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
                "No component in PhyDB with name: " + comp_name);
    comp_ptr->SetLocation(lx, ly);
    comp_ptr->SetPlacementStatus(place_status);
    comp_ptr->SetOrientation(orient);
  }
}

void Dali::ExportWellTapCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &block: circuit_.design().WellTaps()) {
    std::string comp_name = block.Name();
    std::string macro_name = block.TypeName();
    int lx = (int) (block.LLX() * factor_x)
        + circuit_.design().DieAreaOffsetX();
    int ly = (int) (block.LLY() * factor_y)
        + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(block.Status());
    auto orient = phydb::CompOrient(block.Orient());

    auto *phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(phydb_macro_ptr != nullptr,
                "Cannot find " + macro_name + " in PhyDB?!");
    phy_db_ptr_->AddComponent(
        comp_name, phydb_macro_ptr, place_status, lx, ly, orient
    );
  }
}

void Dali::ExportComponentsToPhyDB() {
  ExportOrdinaryComponentsToPhyDB();
  ExportWellTapCellsToPhyDB();
}

void Dali::ExportIoPinsToPhyDB() {
  DaliExpects(!circuit_.Metals().empty(),
              "Need metal layer info to generate PIN location\n");
  for (auto &iopin: circuit_.design().IoPins()) {
    if (!iopin.IsPrePlaced() && iopin.IsPlaced()) {
      DaliExpects(iopin.LayerPtr() != nullptr,
                  "IOPIN metal layer not set? Cannot export it to PhyDB");
      std::string metal_name = iopin.LayerPtr()->Name();
      std::string iopin_name = iopin.Name();
      DaliExpects(phy_db_ptr_->IsIoPinExisting(iopin_name),
                  "IOPIN not in PhyDB? " + iopin_name);
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
        pin_orient = phydb::E;
      } else if (iopin.X() == circuit_.design().RegionRight()) {
        pin_orient = phydb::W;
      } else if (iopin.Y() == circuit_.design().RegionBottom()) {
        pin_orient = phydb::N;
      } else {
        pin_orient = phydb::S;
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
  for (int i = 0; i < well_legalizer_.tot_col_num_; ++i) {
    auto &col = well_legalizer_.col_list_[i];
    for (auto &strip: col.stripe_list_) {
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
        for (auto &cluster: strip.cluster_list_) {
          int row_ly = (int) (cluster.LLY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          int row_uy = (int) (cluster.URY() * factor_y)
              + circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      } else {
        int sz = static_cast<int>(strip.cluster_list_.size());
        for (int j = sz - 1; j >= 0; --j) {
          auto &cluster = strip.cluster_list_[j];
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
