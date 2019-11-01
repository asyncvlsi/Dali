//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PLACERBASE_HPP
#define HPCC_PLACERBASE_HPP

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
  void ReportHPWL(VerboseLevel verbose_level = LOG_INFO);
  void ReportHPWLCtoC();
  void TakeOver(Placer *placer);
  void SanityCheck();

  void GenMATLABScript(std::string const &name_of_file= "block_net_list.m");
  void GenMATLABScriptPlaced(std::string const &name_of_file= "block_net_list.m");
  bool SaveNodeTerminal(std::string const &terminal_file= "terminal.txt", std::string const &node_file= "nodes.txt");
  void SaveDEFFile(std::string const &name_of_file= "circuit.def");
  void SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file);
};

inline void Placer::SetInputCircuit(Circuit *circuit) {
  Assert(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as the input!");
  Assert(!circuit->block_list.empty(), "Invalid input circuit: empty block list!");
  Assert(!circuit->net_list.empty(), "Invalid input circuit: empty net list!");
  circuit_ = circuit;
}

inline Circuit *Placer::GetCircuit() {
  Warning(circuit_ == nullptr, "Circuit is a nullptr!");
  return  circuit_;
}

inline void Placer::SetFillingRate(double rate) {
  Assert((rate <=1) && (rate > 0),"Invalid value: value should be in range (0, 1]");
  filling_rate_ = rate;
}

inline double Placer::FillingRate() const {
  return filling_rate_;
}

inline void Placer::SetAspectRatio(double ratio){
  Assert( ratio >= 0,"Invalid value: value should be in range (0, +infinity)");
  aspect_ratio_ = ratio;
}

inline double Placer::AspectRatio() const {
  return aspect_ratio_;
}

inline void Placer::SetSpaceBlockRatio(double ratio) {
  Assert( ratio >= 1,"Invalid value: value should be in range [1, +infinity)");
  filling_rate_ = 1./ratio;
}

inline double Placer::SpaceBlockRatio() const {
  Warning(filling_rate_ < 1e-3, "Warning: filling rate too small, might lead to large numerical error.");
  return 1.0/filling_rate_;
}

inline std::vector<Block> *Placer::BlockList() {
  return &(circuit_->block_list);
}

inline std::vector<Net> *Placer::NetList() {
  return &(circuit_->net_list);
}

inline int Placer::Left() {
  return left_;
}

inline int Placer::Right() {
  return right_;
}

inline int Placer::Bottom() {
  return bottom_;
}

inline int Placer::Top() {
  return top_;
}

inline void Placer::NetSortBlkPin() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLX!");
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


#endif //HPCC_PLACERBASE_HPP
