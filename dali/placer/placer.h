//
// Created by Yihang Yang on 5/23/19.
//

#ifndef DALI_PLACERBASE_HPP
#define DALI_PLACERBASE_HPP

#include <fstream>
#include <iostream>
#include <string>

#include <config.h>

#include "dali/circuit/circuit.h"
#include "dali/common/memory.h"
#include "dali/common/timing.h"

namespace dali {

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
  virtual ~Placer() = default;

  virtual void LoadConf(std::string const &config_file) {
    BOOST_LOG_TRIVIAL(warning) << "This is a virtual function, which is not supposed to be called directly\n";
  };

  virtual void SetInputCircuit(Circuit *circuit) {
    DaliExpects(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as an input!");
    if (circuit->getBlockList()->empty()) {
      BOOST_LOG_TRIVIAL(info) << "Invalid input circuit: empty block list, nothing to place!\n";
      return;
    }
    if (circuit->getNetList()->empty()) {
      BOOST_LOG_TRIVIAL(info)
        << "Improper input circuit: empty net list, nothing to optimize during placement! But anyway...\n";
    }
    circuit_ = circuit;
  }
  Circuit *GetCircuit() { return circuit_; }
  void SetFillingRate(double rate = 2.0 / 3.0) {
    DaliExpects((rate <= 1) && (rate > 0), "Invalid value: value should be in range (0, 1]");
    filling_rate_ = rate;
  }
  double FillingRate() const { return filling_rate_; }
  void SetAspectRatio(double ratio = 1.0) {
    DaliExpects(ratio >= 0, "Invalid value: value should be in range (0, +infinity)");
    aspect_ratio_ = ratio;
  }
  double AspectRatio() const { return aspect_ratio_; }
  void SetSpaceBlockRatio(double ratio) {
    DaliExpects(ratio >= 1, "Invalid value: value should be in range [1, +infinity)");
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
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot modify any circuits!");
    GetCircuit()->NetSortBlkPin();
  }
  virtual bool StartPlacement() = 0;

  double WeightedHPWLX() {
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot compute WeightedHPWLX!");
    return GetCircuit()->WeightedHPWLX();
  }
  double WeightedHPWLY() {
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot compute WeightedHPWLY!");
    return GetCircuit()->WeightedHPWLY();
  }
  double WeightedHPWL() {
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
    return GetCircuit()->WeightedHPWL();
  }

  void ReportHPWL() {
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
    circuit_->ReportHPWL();
  }
  static void ReportMemory() {
    auto peak_mem = getPeakRSS();
    auto curr_mem = getCurrentRSS();
    BOOST_LOG_TRIVIAL(info) << "(peak memory: "
                            << (peak_mem >> 20u) << " MB, "
                            << " current memory: "
                            << (curr_mem >> 20u) << " MB)\n";
  }

  void ReportHPWLCtoC() {
    DaliExpects(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLCtoC!");
    GetCircuit()->ReportHPWLCtoC();
  }

  void TakeOver(Placer *placer);
  void SanityCheck();
  void UpdateMovableBlkPlacementStatus();

  void SimpleIoPinPlacement(std::string metal_layer);

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

}

#endif //DALI_PLACERBASE_HPP
