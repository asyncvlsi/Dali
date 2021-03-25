//
// Created by Yihang Yang on 3/23/21.
//

#include "dali.h"

namespace dali {

Dali::Dali(phydb::PhyDB *phy_db_ptr, boost::log::trivial::severity_level sl) {
  InitLogging(sl);
  phy_db_ptr_ = phy_db_ptr;
  circuit_.InitializeFromPhyDB(phy_db_ptr);
}

void Dali::StartPlacement(double density, int number_of_threads) {
  int num_of_thread_openmp = number_of_threads;
  omp_set_num_threads(num_of_thread_openmp);
  Eigen::initParallel();
  BOOST_LOG_TRIVIAL(info)  <<"Eigen thread " << Eigen::nbThreads() << "\n";

  BOOST_LOG_TRIVIAL(info)  << "  Average white space utility: " << circuit_.WhiteSpaceUsage() << std::endl;
  circuit_.ReportBriefSummary();
  //circuit.ReportBlockType();
  //circuit.ReportIOPin();
  circuit_.ReportHPWL();
  //circuit.BuildBlkPairNets();

  //std::string config_file = "dali.conf";

  GPSimPL gb_placer;
  //gb_placer.LoadConf(config_file);
  gb_placer.SetInputCircuit(&circuit_);
  gb_placer.is_dump = false;
  gb_placer.SetBoundaryDef();
  gb_placer.SetFillingRate(density);
  gb_placer.ReportBoundaries();
  gb_placer.StartPlacement();
  //gb_placer.SaveDEFFile("benchmark_1K_dali.def", def_file_name);
  gb_placer.GenMATLABTable("gb_result.txt");
  //circuit.GenLongNetTable("gb_longnet.txt");
  //gb_placer->GenMATLABWellTable("gb_result");
  //circuit.ReportNetFanoutHisto();

  LGTetrisEx legalizer;
  legalizer.TakeOver(&gb_placer);
  legalizer.IsPrintDisplacement(true);
  legalizer.StartPlacement();
  legalizer.GenMATLABTable("lg_result.txt");
  legalizer.GenDisplacement("disp_result.txt");
  //circuit.GenLongNetTable("lg_longnet.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

  StdClusterWellLegalizer std_cluster_well_legalizer;
  std_cluster_well_legalizer.TakeOver(&gb_placer);
  std_cluster_well_legalizer.SetStripPartitionMode(SCAVENGE);
  std_cluster_well_legalizer.StartPlacement();
  //std_cluster_well_legalizer.GenMatlabClusterTable("sc_result");
  std_cluster_well_legalizer.GenMATLABTable("sc_result.txt");
  std_cluster_well_legalizer.GenMatlabClusterTable("sc_result");
  std_cluster_well_legalizer.GenMATLABWellTable("scw", 0);
  //circuit.GenLongNetTable("sc_longnet.txt");

  //std_cluster_well_legalizer.SimpleIOPinPlacement(0);
  std_cluster_well_legalizer.EmitDEFWellFile("circuit", 1);
}

void Dali::ExportToPhyDB() {
  // 1. update component locations
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  int cell_count = 0;

  // 1.a existing components
  for (auto &block: circuit_.BlockListRef()) {
    if (block.TypePtr() == circuit_.getTechRef().io_dummy_blk_type_ptr_) continue;
    std::string comp_name = block.Name();
    int lx = (int) (block.LLX() * factor_x) + circuit_.getDesignRef().die_area_offset_x_;
    int ly = (int) (block.LLY() * factor_y) + circuit_.getDesignRef().die_area_offset_y_;
    auto place_status = phydb::PlaceStatus(block.PlacementStatus());
    auto orient = phydb::CompOrient(block.Orient());

    phydb::Component *comp_ptr = phy_db_ptr_->GetComponentPtr(comp_name);
    DaliExpects(comp_ptr!= nullptr, "No component in PhyDB with name: " + comp_name);
    comp_ptr->SetLocation(lx ,ly);
    comp_ptr->SetPlacementStatus(place_status);
    comp_ptr->SetOrientation(orient);
  }

  // 1.b well tap cells
  for (auto &block: circuit_.getDesignRef().well_tap_list) {
    std::string comp_name = block.Name();
    std::string macro_name = *(block.TypePtr()->NamePtr());
    int lx = (int) (block.LLX() * factor_x) + circuit_.getDesignRef().die_area_offset_x_;
    int ly = (int) (block.LLY() * factor_y) + circuit_.getDesignRef().die_area_offset_y_;
    auto place_status = phydb::PlaceStatus(block.PlacementStatus());
    auto orient = phydb::CompOrient(block.Orient());

    phy_db_ptr_->AddComponent(comp_name, macro_name, place_status, lx, ly, orient);
  }


  // TODO 2. IOPINs

  // TODO 3. Mini-rows
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

}
