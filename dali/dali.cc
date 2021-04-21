//
// Created by Yihang Yang on 3/23/21.
//

#include "dali.h"

namespace dali {

Dali::Dali(phydb::PhyDB *phy_db_ptr, std::string sl) {
  auto boost_sl = StrToLoggingLevel(sl);
  InitLogging(boost_sl);
  phy_db_ptr_ = phy_db_ptr;
  circuit_.InitializeFromPhyDB(phy_db_ptr);
}

Dali::Dali(phydb::PhyDB *phy_db_ptr, boost::log::trivial::severity_level sl) {
  InitLogging(sl);
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

  //std::string config_file = "dali.conf";

  //gb_placer_.LoadConf(config_file);
  gb_placer_.SetInputCircuit(&circuit_);
  gb_placer_.is_dump = false;
  gb_placer_.SetBoundaryDef();
  gb_placer_.SetFillingRate(density);
  gb_placer_.ReportBoundaries();
  gb_placer_.StartPlacement();
  //gb_placer_.SaveDEFFile("benchmark_1K_dali.def", def_file_name);
  gb_placer_.GenMATLABTable("gb_result.txt");
  //circuit.GenLongNetTable("gb_longnet.txt");
  //gb_placer_->GenMATLABWellTable("gb_result");
  //circuit.ReportNetFanoutHisto();

  /*
  legalizer_.TakeOver(&gb_placer_);
  legalizer_.IsPrintDisplacement(true);
  legalizer_.StartPlacement();
  legalizer_.GenMATLABTable("lg_result.txt");
  legalizer_.GenDisplacement("disp_result.txt");
  //circuit.GenLongNetTable("lg_longnet.txt");
  //legalizer_->SaveDEFFile("circuit.def", def_file);
  */

  well_legalizer_.TakeOver(&gb_placer_);
  well_legalizer_.SetStripePartitionMode(SCAVENGE);
  well_legalizer_.StartPlacement();
  //well_legalizer_.GenMatlabClusterTable("sc_result");
  well_legalizer_.GenMATLABTable("sc_result.txt");
  well_legalizer_.GenMatlabClusterTable("sc_result");
  well_legalizer_.GenMATLABWellTable("scw", 0);
  //circuit.GenLongNetTable("sc_longnet.txt");

  //well_legalizer_.SimpleIoPinPlacement(0);
  well_legalizer_.EmitDEFWellFile("circuit", 1);
}

void Dali::SimpleIoPinPlacement(std::string metal_layer) {
  well_legalizer_.SimpleIoPinPlacement(metal_layer);
}

void Dali::ExportToPhyDB() {
  // 1. COMPONENTS
  ExportComponentsToPhyDB();
  // 2. IOPINs
  ExportIoPinsToPhyDB();
  // 3. MiniRows
  ExportMiniRowsToPhyDB();
  // TODO 4. NPPP and Well
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
