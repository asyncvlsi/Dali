//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_PLACERBASE_HPP
#define DALI_PLACERBASE_HPP

#include <string>
#include <iostream>
#include <fstream>
#include "../circuit/circuit.h"
#include "../common/global.h"

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

  double GetBlkHPWL(Block &blk);

public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer();

  void SetInputCircuit(Circuit *circuit);
  Circuit *GetCircuit() {return circuit_;}
  void SetFillingRate(double rate = 2.0/3.0);
  double FillingRate() const {return filling_rate_;}
  void SetAspectRatio(double ratio = 1.0);
  double AspectRatio() const {return aspect_ratio_;}
  void SetSpaceBlockRatio(double ratio);

  std::vector<Block> *BlockList() {return &(circuit_->block_list);};
  std::vector<Net> *NetList() {return &(circuit_->net_list);};

  bool IsBoundaryProper();
  void SetBoundaryAuto();
  void SetBoundary(int left, int right, int bottom, int top);
  void SetBoundaryDef();
  void ReportBoundaries();
  int Left() {return left_;};
  int Right() {return right_;};
  int Bottom() {return bottom_;};
  int Top() {return top_;};
  void UpdateAspectRatio();
  void NetSortBlkPin();
  virtual void StartPlacement() = 0;
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL(VerboseLevel verbose_level = LOG_INFO);
  void ReportHPWLCtoC();
  void TakeOver(Placer *placer);
  void SanityCheck();
  void UpdateComponentsPlacementStatus();
  void IOPinPlacement();

  void GenMATLABTable(std::string const &name_of_file = "block.txt") {circuit_->GenMATLABTable(name_of_file);}
  void GenMATLABWellTable(std::string const &name_of_file = "res") {circuit_->GenMATLABWellTable(name_of_file);}
  void GenMATLABScriptPlaced(std::string const &name_of_file = "block_net_list.m");
  bool SaveNodeTerminal(std::string const &terminal_file = "terminal.txt", std::string const &node_file = "nodes.txt");
  void SaveDEFFile(std::string const &name_of_file = "circuit.def");
  void SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file);
};

inline void Placer::SetInputCircuit(Circuit *circuit) {
  Assert(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as an input!");
  if (circuit->block_list.empty()) {
    std::cout << "Invalid input circuit: empty block list, nothing to place!\n";
    return;
  }
  if (circuit->net_list.empty()) {
    std::cout << "Improper input circuit: empty net list, nothing to optimize during placement! But anyway...\n";
    return;
  }
  circuit_ = circuit;
}

inline void Placer::SetFillingRate(double rate) {
  Assert((rate <=1) && (rate > 0),"Invalid value: value should be in range (0, 1]");
  filling_rate_ = rate;
}

inline void Placer::SetAspectRatio(double ratio){
  Assert( ratio >= 0,"Invalid value: value should be in range (0, +infinity)");
  aspect_ratio_ = ratio;
}

inline void Placer::SetSpaceBlockRatio(double ratio) {
  Assert( ratio >= 1,"Invalid value: value should be in range [1, +infinity)");
  filling_rate_ = 1./ratio;
}

inline void Placer::NetSortBlkPin() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot modify any circuits!");
  GetCircuit()->NetSortBlkPin();
}

inline double Placer::HPWLX() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLX!");
  return GetCircuit()->HPWLX();
}

inline double Placer::HPWLY() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLY!");
  return GetCircuit()->HPWLY();
}

inline double Placer::HPWL() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
  return GetCircuit()->HPWL();
}

inline void Placer::ReportHPWL(VerboseLevel verbose_level) {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
  if (globalVerboseLevel >= verbose_level) {
    GetCircuit()->ReportHPWL();
  }
}

inline void Placer::ReportHPWLCtoC() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLCtoC!");
  GetCircuit()->ReportHPWLCtoC();
}


#endif //DALI_PLACERBASE_HPP
