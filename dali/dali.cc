//
// Created by Yihang Yang on 3/23/21.
//

#include "dali.h"

#include <cstdlib>

namespace dali {

Dali::Dali(phydb::PhyDB *phy_db_ptr, std::string sl) {
    auto boost_sl = StrToLoggingLevel(sl);
    std::string log_file_name;
    InitLogging(log_file_name, false, boost_sl);
    phy_db_ptr_ = phy_db_ptr;
    circuit_.InitializeFromPhyDB(phy_db_ptr);
}

Dali::Dali(phydb::PhyDB *phy_db_ptr, boost::log::trivial::severity_level sl) {
    std::string log_file_name;
    InitLogging(log_file_name, false, sl);
    phy_db_ptr_ = phy_db_ptr;
    circuit_.InitializeFromPhyDB(phy_db_ptr);
}

void Dali::StartPlacement(double density, int number_of_threads) {
    int num_of_thread_openmp = number_of_threads;
    omp_set_num_threads(num_of_thread_openmp);
    Eigen::initParallel();
    BOOST_LOG_TRIVIAL(info) << "Eigen thread " << Eigen::nbThreads() << "\n";

    BOOST_LOG_TRIVIAL(info) << "  Average white space utility: " << circuit_.WhiteSpaceUsage() << std::endl;
    circuit_.ReportBriefSummary();
    //circuit.ReportBlockType();
    //circuit.ReportIOPin();
    circuit_.ReportHPWL();
    //circuit.BuildBlkPairNets();

    GlobalPlace(density);
    UnifiedLegalization();
}

void Dali::SimpleIoPinPlacement(std::string metal_layer) {
    well_legalizer_.SimpleIoPinPlacement(metal_layer);
}

void Dali::AddWellTaps(phydb::Macro *cell, double cell_interval_microns, bool is_checker_board) {
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

    //well_legalizer_.SimpleIoPinPlacement(0);
    well_legalizer_.EmitDEFWellFile("circuit", 1);
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
void Dali::ExternalDetailedPlaceAndLegalize(std::string engine, bool load_dp_result) {
    // create a script for detailed placement and legalization
    BOOST_LOG_TRIVIAL(info) << "Creating detailed placement and legalization script...\n";
    std::string dp_script_name = "dali_" + engine + ".cmd";
    std::string legal_def_file = CreateDetailedPlacementAndLegalizationScript(engine, dp_script_name);

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
    // 3. MiniRows
    ExportMiniRowsToPhyDB();
    // TODO 4. NP/PP and Well
    ExportNpPpWellToPhyDB();
}

void Dali::Close() {
    CloseLogging();
}

void Dali::ExportToDEF(std::string &input_def_file_full_name, std::string output_def_name) {
    circuit_.SaveDefFile(output_def_name, "", input_def_file_full_name, 1, 1, 2, 1);
    circuit_.SaveDefFile(output_def_name, "_io", input_def_file_full_name, 1, 1, 1, 1);
    circuit_.SaveDefFile(output_def_name, "_filling", input_def_file_full_name, 1, 4, 2, 1);
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
std::string Dali::CreateDetailedPlacementAndLegalizationScript(std::string &engine,
                                                               std::string &script_name) {
    // create script for innovus
    DaliExpects(engine == "innovus", "Only support Innovus now");
    std::ofstream ost(script_name);
    ost << "loadLefFile " << phy_db_ptr_->GetTechPtr()->GetLefName() << "\n";
    std::string input_def_file = phy_db_ptr_->GetDesignPtr()->GetDefName();
    std::string tmp_def_out = "dali_global_" + phy_db_ptr_->GetDesignPtr()->GetName();
    ExportToDEF(input_def_file, tmp_def_out);
    ost << "loadDefFile " << tmp_def_out << ".def\n";
    ost << "refinePlace\n";
    std::string out_def = "dali_global_innovus_refine_" + phy_db_ptr_->GetDesignPtr()->GetName() + ".def";
    ost << "defOut " << out_def << "\n";

    // check if the engine can be found
    DaliExpects(IsExecutableExisting(engine), "Cannot find the given engine");
    return out_def;
}

void Dali::ExportComponentsToPhyDB() {
    double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
    double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();

    // a. existing components
    for (auto &block: circuit_.BlockListRef()) {
        if (block.TypePtr() == circuit_.getTechRef().io_dummy_blk_type_ptr_) continue;
        std::string comp_name = block.Name();
        int lx = (int) (block.LLX() * factor_x) + circuit_.getDesignRef().die_area_offset_x_;
        int ly = (int) (block.LLY() * factor_y) + circuit_.getDesignRef().die_area_offset_y_;
        auto place_status = phydb::PlaceStatus(block.PlacementStatus());
        auto orient = phydb::CompOrient(block.Orient());

        phydb::Component *comp_ptr = phy_db_ptr_->GetComponentPtr(comp_name);
        DaliExpects(comp_ptr != nullptr, "No component in PhyDB with name: " + comp_name);
        comp_ptr->SetLocation(lx, ly);
        comp_ptr->SetPlacementStatus(place_status);
        comp_ptr->SetOrientation(orient);
    }

    // b. well tap cells
    for (auto &block: circuit_.getDesignRef().well_tap_list) {
        std::string comp_name = block.Name();
        std::string macro_name = *(block.TypePtr()->NamePtr());
        int lx = (int) (block.LLX() * factor_x) + circuit_.getDesignRef().die_area_offset_x_;
        int ly = (int) (block.LLY() * factor_y) + circuit_.getDesignRef().die_area_offset_y_;
        auto place_status = phydb::PlaceStatus(block.PlacementStatus());
        auto orient = phydb::CompOrient(block.Orient());

        phy_db_ptr_->AddComponent(comp_name, macro_name, place_status, lx, ly, orient);
    }
}

void Dali::ExportIoPinsToPhyDB() {
    DaliExpects(!circuit_.getTechRef().metal_list_.empty(), "Need metal layer info to generate PIN location\n");
    double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
    double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();

    for (auto &iopin: circuit_.getDesignRef().iopin_list) {
        if (!iopin.IsPrePlaced() && iopin.IsPlaced()) {
            std::string metal_name = *(iopin.Layer()->Name());
            int half_width = std::ceil(iopin.Layer()->MinHeight() / 2.0 * circuit_.DistanceMicrons());
            int height = std::ceil(iopin.Layer()->Width() * circuit_.DistanceMicrons());

            std::string iopin_name = *(iopin.Name());
            DaliExpects(phy_db_ptr_->IsIoPinExisting(iopin_name), "IOPIN not in PhyDB? " + iopin_name);
            phydb::IOPin *iopin_ptr = phy_db_ptr_->GetIoPinPtr(iopin_name);
            iopin_ptr->SetShape(metal_name, -half_width, 0, half_width, height);

            int pin_x = (int) (iopin.X() * factor_x) + circuit_.getDesignRef().die_area_offset_x_;
            int pin_y = (int) (iopin.Y() * factor_y) + circuit_.getDesignRef().die_area_offset_y_;
            phydb::CompOrient pin_orient;
            if (iopin.X() == circuit_.getDesignRef().region_left_) {
                pin_orient = phydb::E;
            } else if (iopin.X() == circuit_.getDesignRef().region_right_) {
                pin_orient = phydb::W;
            } else if (iopin.Y() == circuit_.getDesignRef().region_bottom_) {
                pin_orient = phydb::N;
            } else {
                pin_orient = phydb::S;
            }
            iopin_ptr->SetPlacement(phydb::PLACED, pin_x, pin_y, pin_orient);
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
            phydb::ClusterCol *col_ptr = phy_db_ptr_->AddClusterCol(column_name, bot_signal_);

            int col_lx = (int) (strip.LLX() * factor_x) + circuit_.getDesign()->die_area_offset_x_;
            int col_ux = (int) (strip.URX() * factor_x) + circuit_.getDesign()->die_area_offset_x_;
            col_ptr->SetXRange(col_lx, col_ux);

            if (strip.is_bottom_up_) {
                for (auto &cluster: strip.cluster_list_) {
                    int row_ly = (int) (cluster.LLY() * factor_y) + circuit_.getDesign()->die_area_offset_y_;
                    int row_uy = (int) (cluster.URY() * factor_y) + circuit_.getDesign()->die_area_offset_y_;
                    col_ptr->AddRow(row_ly, row_uy);
                }
            } else {
                int sz = strip.cluster_list_.size();
                for (int j = sz - 1; j >= 0; --j) {
                    auto &cluster = strip.cluster_list_[j];
                    int row_ly = (int) (cluster.LLY() * factor_y) + circuit_.getDesign()->die_area_offset_y_;
                    int row_uy = (int) (cluster.URY() * factor_y) + circuit_.getDesign()->die_area_offset_y_;
                    col_ptr->AddRow(row_ly, row_uy);
                }
            }
        }
    }
}

void Dali::ExportNpPpWellToPhyDB() {

}

}
