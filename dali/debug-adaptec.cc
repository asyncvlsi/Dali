//
// Created by Yihang Yang on 2/6/20
// this is for ISPD2005/2006 benchmark suite debugging
//

#include <ctime>

#include <iostream>

#include <config.h>
#include <phydb/phydb.h>

#include "circuit.h"
#include "common/logging.h"
#include "common/si2lefdef.h"
#include "placer.h"

#define TEST_LG 0
#define TEST_WLG 0

using namespace dali;

int main(int argc, char **argv) {
  InitLogging(boost::log::trivial::trace);

  double tune_param;
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if ((arg == "-param") && i < argc) {
      std::string tmp_str = std::string(argv[i++]);
      try {
        tune_param = std::stod(tmp_str);
      } catch (...) {
        std::cout << "Invalid value!\n";
        return 1;
      }
    } else {
      std::cout << "Unknown options\n";
      std::cout << arg << "\n";
      return 1;
    }
  }

  //std::string config_file = "dali.conf";
  //config_read(config_file.c_str());
  //std::string dump_file = "dali_dump.conf";
  //FILE *fp = fopen(dump_file.c_str(), "w+");
  //config_dump(fp);

  int num_of_thread_openmp = 1;
  omp_set_num_threads(num_of_thread_openmp);

  Eigen::initParallel();
  //Eigen::setNbThreads(1);
  BOOST_LOG_TRIVIAL(info)  <<"Eigen thread " << Eigen::nbThreads() << "\n";

  time_t Time = clock();
  double wall_time = get_wall_time();

  std::string adaptec1_lef = "ISPD2005/bigblue4.lef";
#if TEST_LG
  std::string adaptec1_def = "adaptec1_pl.def";
#else
  std::string adaptec1_def = "ISPD2005/bigblue4.def";
#endif

  phydb::PhyDB phy_db;
  phy_db.SetPlacementGrids(0.01, 0.01);
  phy_db.ReadLef(adaptec1_lef);
  phy_db.ReadDef(adaptec1_def);
  Placer::ReportMemory();

  Circuit circuit;
  circuit.InitializeFromPhyDB(&phy_db);
  //circuit.SetGridValue(0.01, 0.01);
  //circuit.ReadLefFile(adaptec1_lef);
  //circuit.ReadDefFile(adaptec1_def);
  //circuit.ReportBlockType();
  //circuit.ReportBlockList();
  //circuit.ReportNetList();
  //circuit.GenMATLABTable("_result.txt");

  BOOST_LOG_TRIVIAL(info) << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s" << std::endl;

  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  GPSimPL gb_placer;
  //gb_placer_.net_model_update_stop_criterion_ = tune_param;
  BOOST_LOG_TRIVIAL(info) << "tune_param: " << tune_param << "\n";
  gb_placer.SetInputCircuit(&circuit);
  gb_placer.SetBoundaryDef();
  gb_placer.SetFillingRate(1);
  gb_placer.ReportBoundaries();
  gb_placer.is_dump = false;
#if !TEST_LG
  gb_placer.StartPlacement();
  //gb_placer_.SaveDEFFile("adaptec1_pl.def", adaptec1_def);
  circuit.SaveBookshelfPl("adaptec1bs.pl");
#endif
  gb_placer.GenMATLABTable("gb_result.txt");

  /*
  LGTetrisEx legalizer_;
  legalizer_.TakeOver(&gb_placer_);
  legalizer_.SetRowHeight(12);
  legalizer_.StartPlacement();
  //legalizer_.PlotAvailSpace("as_result.txt");
  legalizer_.GenMATLABTable("lg_result.txt");
  //legalizer_->SaveDEFFile("circuit.def", def_file);
   */

#if TEST_WLG
  circuit.LoadImaginaryCellFile();
  //circuit.ReportWellShape();
  auto *well_legalizer = new StdClusterWellLegalizer;
  well_legalizer->TakeOver(&gb_placer_);
  well_legalizer->SetRowHeight(1);
  well_legalizer->StartPlacement();
  well_legalizer->GenMATLABTable("sc_result.txt");
  well_legalizer->GenMATLABWellTable("scw");
  well_legalizer->EmitDEFWellFile("circuit", adaptec1_def);
  delete well_legalizer;
#endif

  //circuit.InitNetFanoutHistogram();
  //circuit.ReportNetFanoutHistogram();
  //circuit.ReportHPWLHistogramLinear();
  //circuit.ReportHPWLHistogramLogarithm();

  Time = clock() - Time;
  wall_time = get_wall_time() - wall_time;
  BOOST_LOG_TRIVIAL(info) << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";
  BOOST_LOG_TRIVIAL(info) << "Wall time " << wall_time << "s.\n";
  BOOST_LOG_TRIVIAL(info) << "tune_param: " << tune_param << "\n";

  return 0;
}
