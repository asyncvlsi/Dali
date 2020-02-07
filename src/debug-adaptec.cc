//
// Created by Yihang Yang on 2/6/20
// this is for ISPD2005/2006 benchmark suite debugging
//

#include <ctime>

#include <iostream>

#include "circuit.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

int main() {
  Circuit circuit;

  time_t Time = clock();

  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  circuit.SetGridValue(0.01, 0.01);
  circuit.ReadLefFile(adaptec1_lef);
  circuit.ReadDefFile(adaptec1_def);
  circuit.def_left = 459;
  circuit.def_right = 10692 + 459;
  circuit.def_bottom = 459;
  circuit.def_top = 11127 + 12;

  std::cout << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << "s\n";

  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  GPSimPL gb_placer;
  gb_placer.SetInputCircuit(&circuit);
  gb_placer.SetBoundaryDef();
  gb_placer.SetFillingRate(1);
  gb_placer.ReportBoundaries();
  gb_placer.is_dump = true;
  gb_placer.StartPlacement();
  gb_placer.GenMATLABTable("gb_result.txt");
  gb_placer.SaveDEFFile("adaptec1_pl.def", adaptec1_def);
  circuit.SaveBookshelfPl("adaptec1bs.pl");

  LGTetrisEx legalizer;
  legalizer.TakeOver(&gb_placer);
  legalizer.SetRowHeight(12);
  legalizer.StartPlacement();
  legalizer.GenAvailSpace("as_result.txt");
  legalizer.GenMATLABTable("lg_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
