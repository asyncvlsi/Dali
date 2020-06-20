//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_PLACERBASE_HPP
#define DALI_PLACERBASE_HPP

#include <fstream>
#include <iostream>
#include <string>

#include "circuit/circuit.h"
#include "common/global.h"
#include "common/memory.h"
#include "common/timing.h"

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
  Circuit *circuit_;

  double GetBlkHPWL(Block &blk);

 public:
  Placer();
  Placer(double aspect_ratio, double filling_rate);
  virtual ~Placer();

  virtual void SetInputCircuit(Circuit *circuit) {
    Assert(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as an input!");
    if (circuit->getBlockList()->empty()) {
      std::cout << "Invalid input circuit: empty block list, nothing to place!\n";
      return;
    }
    if (circuit->getNetList()->empty()) {
      std::cout << "Improper input circuit: empty net list, nothing to optimize during placement! But anyway...\n";
    }
    circuit_ = circuit;
  }
  Circuit *GetCircuit() { return circuit_; }
  void SetFillingRate(double rate = 2.0 / 3.0) {
    Assert((rate <= 1) && (rate > 0), "Invalid value: value should be in range (0, 1]");
    filling_rate_ = rate;
  }
  double FillingRate() const { return filling_rate_; }
  void SetAspectRatio(double ratio = 1.0) {
    Assert(ratio >= 0, "Invalid value: value should be in range (0, +infinity)");
    aspect_ratio_ = ratio;
  }
  double AspectRatio() const { return aspect_ratio_; }
  void SetSpaceBlockRatio(double ratio) {
    Assert(ratio >= 1, "Invalid value: value should be in range [1, +infinity)");
    filling_rate_ = 1. / ratio;
  }

  std::vector<Block> *BlockList() { return circuit_->getBlockList(); }
  std::vector<Net> *NetList() { return circuit_->getNetList(); }

  bool IsBoundaryProper();
  void SetBoundaryAuto();
  void SetBoundary(int left, int right, int bottom, int top);
  void SetBoundaryDef();
  void ReportBoundaries();

  int RegionLeft() const { return left_; }
  int RegionRight() const { return right_; }
  int RegionBottom() const { return bottom_; }
  int RegionTop() const { return top_; }
  int RegionWidth() const { return right_ - left_; }
  int RegionHeight() const { return top_ - bottom_; }

  void UpdateAspectRatio();
  void NetSortBlkPin() {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot modify any circuits!");
    GetCircuit()->NetSortBlkPin();
  }
  virtual bool StartPlacement() = 0;

  double HPWLX() {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLX!");
    return GetCircuit()->HPWLX();
  }
  double HPWLY() {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLY!");
    return GetCircuit()->HPWLY();
  }
  double HPWL() {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
    return GetCircuit()->HPWL();
  }

  void ReportHPWL(VerboseLevel verbose_level = LOG_INFO) {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
    if (globalVerboseLevel >= verbose_level) {
      GetCircuit()->ReportHPWL();
    }
  }
  static void ReportMemory(VerboseLevel verbose_level = LOG_INFO) {
    if (globalVerboseLevel >= verbose_level) {
      auto peak_mem = getPeakRSS();
      auto curr_mem = getCurrentRSS();
      std::cout << "(peak memory: "
                << (peak_mem >> 20u) << " MB, "
                << " current memory: "
                << (curr_mem >> 20u) << " MB)\n";
    }
  }

  void ReportHPWLCtoC() {
    Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLCtoC!");
    GetCircuit()->ReportHPWLCtoC();
  }

  void TakeOver(Placer *placer);
  void SanityCheck();
  void UpdateMovableBlkPlacementStatus();

  void SimpleIOPinPlacement(int pin_metal_layer);

  /****File I/O member functions****/
  void GenMATLABTable(std::string const &name_of_file) {
    circuit_->GenMATLABTable(name_of_file);
  }
  virtual void GenMATLABWellTable(std::string const &name_of_file, int well_emit_mode) {
    circuit_->GenMATLABWellTable(name_of_file);
  }
  void GenMATLABScriptPlaced(std::string const &name_of_file = "block_net_list.m");
  bool SaveNodeTerminal(std::string const &terminal_file = "terminal.txt", std::string const &node_file = "nodes.txt");
  void SaveDEFFile(std::string const &name_of_file = "circuit.def");
  void SaveDEFFile(std::string const &name_of_file, std::string const &input_def_file);
  virtual void EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode);

  /****for testing purposes****/
  void ShiftX(double shift_x);
  void ShiftY(double shift_y);
};

#endif //DALI_PLACERBASE_HPP
