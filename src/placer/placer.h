//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PLACERBASE_HPP
#define HPCC_PLACERBASE_HPP

#include <string>
#include <iostream>
#include <fstream>
#include "circuit/circuit.h"
#include "common/misc.h"

class Placer {
 protected:
  /* essential data entries */
  double aspect_ratio_; // placement region Height/Width
  double filling_rate_;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the aspect_ratio_ or filling_rate_ might also be changed */
  int left_, right_, bottom_, top_;
  // boundaries of the placement region
  Circuit* circuit_;

public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer();

  void SetInputCircuit(Circuit *circuit);
  Circuit *GetCircuit();
  void SetFillingRate(double rate = 2.0/3.0);
  double FillingRate() const;
  void SetAspectRatio(double ratio = 1.0);
  double AspectRatio() const;
  void SetSpaceBlockRatio(double ratio);
  double SpaceBlockRatio() const;

  std::vector<Block> *BlockList();
  std::vector<Net> *NetList();

  bool IsBoundaryProper();
  void SetBoundaryAuto();
  void SetBoundary(int left, int right, int bottom, int top);
  void SetBoundaryDef();
  void ReportBoundaries();
  int Left();
  int Right();
  int Bottom();
  int Top();
  bool UpdateAspectRatio();
  void NetSortBlkPin();
  virtual void StartPlacement() = 0;
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL();
  void ReportHPWLCtoC();
  void TakeOver(Placer *placer);
  void SanityCheck();

  void GenMATLABScript(std::string const &name_of_file= "block_net_list.m");
  bool SaveNodeTerminal(std::string const &terminal_file= "terminal.txt", std::string const &node_file= "nodes.txt");
  void SaveDEFFile(std::string const &name_of_file= "circuit.def");
  void SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file);
};


#endif //HPCC_PLACERBASE_HPP
