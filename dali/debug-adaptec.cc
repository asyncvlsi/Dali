//
// Created by Yihang Yang on 2/6/20
// this is for ISPD2005/2006 benchmark suite debugging
//

#include <ctime>

#include <iostream>

#include <config.h>

#include "circuit.h"
#include "common/logging.h"
#include "common/si2lefdef.h"
#include "placer.h"

#define TEST_LG 0
#define TEST_WLG 0

using namespace dali;

int main(int argc, char **argv) {
  init_logging(boost::log::trivial::trace);
  Circuit circuit;

  //std::string config_file = "dali.conf";
  //config_read(config_file.c_str());
  //std::string dump_file = "dali_dump.conf";
  //FILE *fp = fopen(dump_file.c_str(), "w+");
  //config_dump(fp);

  int num_of_thread_openmp = 1;
  omp_set_num_threads(num_of_thread_openmp);

  Eigen::initParallel();
  //Eigen::setNbThreads(1);
  BOOST_LOG_TRIVIAL(info)  <<"Eigen thread " << Eigen::nbThreads();

  double wall_time_start = get_wall_time();

  std::string adaptec1_lef = "ISPD2005/adaptec1.lef";
#if TEST_LG
  std::string adaptec1_def = "adaptec1_pl.def";
#else
  std::string adaptec1_def = "ISPD2005/adaptec1.def";
#endif

  circuit.setGridValue(0.01, 0.01);
  readLef(adaptec1_lef, circuit);
  readDef(adaptec1_def, circuit);
  //circuit.ReportBlockType();
  //circuit.ReportBlockList();
  //circuit.ReportNetList();

  //circuit.getDesign()->region_left_ = 459;
  //circuit.getDesign()->region_right_ = 10692 + 459;
  //circuit.getDesign()->region_bottom_ = 459;
  //circuit.getDesign()->region_top_ = 11127 + 12;
  //circuit.GenMATLABTable("_result.txt");

  BOOST_LOG_TRIVIAL(info) << "File loading complete, time: " << get_wall_time() - wall_time_start << " s";

  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  GPSimPL gb_placer;
  gb_placer.SetInputCircuit(&circuit);
  gb_placer.SetBoundaryDef();
  gb_placer.SetFillingRate(1);
  gb_placer.ReportBoundaries();
  //gb_placer.is_dump = true;
#if !TEST_LG
  gb_placer.StartPlacement();
  gb_placer.SaveDEFFile("adaptec1_pl.def", adaptec1_def);
  circuit.SaveBookshelfPl("adaptec1bs.pl");
#endif
  gb_placer.GenMATLABTable("gb_result.txt");

  /*
  LGTetrisEx legalizer;
  legalizer.TakeOver(&gb_placer);
  legalizer.SetRowHeight(12);
  legalizer.StartPlacement();
  //legalizer.GenAvailSpace("as_result.txt");
  legalizer.GenMATLABTable("lg_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);
   */

#if TEST_WLG
  circuit.LoadImaginaryCellFile();
  //circuit.ReportWellShape();
  auto *well_legalizer = new StdClusterWellLegalizer;
  well_legalizer->TakeOver(&gb_placer);
  well_legalizer->SetRowHeight(1);
  well_legalizer->StartPlacement();
  well_legalizer->GenMATLABTable("sc_result.txt");
  well_legalizer->GenMATLABWellTable("scw");
  well_legalizer->EmitDEFWellFile("circuit", adaptec1_def);
  delete well_legalizer;
#endif

  BOOST_LOG_TRIVIAL(info) << "Execution time " << get_wall_time() - wall_time_start << "s";

  return 0;
}
