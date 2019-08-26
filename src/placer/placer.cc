//
// Created by Yihang Yang on 2019-05-23.
//

#include <cmath>
#include "placer.h"

Placer::Placer() {
  aspect_ratio_ = 0;
  filling_rate_ = 0;
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  circuit_ = nullptr;
}

Placer::Placer(double aspect_ratio, double filling_rate) : aspect_ratio_(aspect_ratio), filling_rate_(filling_rate) {
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  circuit_ = nullptr;
}

Placer::~Placer() {}

void Placer::SetInputCircuit(Circuit *circuit) {
  Assert(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as the input!");
  Assert(!circuit->block_list.empty(), "Invalid input circuit: empty block list!");
  Assert(!circuit->net_list.empty(), "Invalid input circuit: empty net list!");
  circuit_ = circuit;
}

Circuit *Placer::GetCircuit() {
  Warning(circuit_ == nullptr, "Circuit is a nullptr!");
  return  circuit_;
}

void Placer::SetFillingRate(double rate) {
  Assert((rate <=1) && (rate > 0),"Invalid value: value should be in range (0, 1]");
  filling_rate_ = rate;
}

double Placer::FillingRate() const {
  return filling_rate_;
}

void Placer::SetAspectRatio(double ratio){
  Assert( ratio >= 0,"Invalid value: value should be in range (0, +infinity)");
  aspect_ratio_ = ratio;
}

double Placer::AspectRatio() const {
  return aspect_ratio_;
}

void Placer::SetSpaceBlockRatio(double ratio) {
  Assert( ratio >= 1,"Invalid value: value should be in range [1, +infinity)");
  filling_rate_ = 1./ratio;
}

double Placer::SpaceBlockRatio() const {
  Warning(filling_rate_ < 1e-3, "Warning: filling rate too small, might lead to large numerical error.");
  return 1.0/filling_rate_;
}

std::vector<Block> *Placer::BlockList() {
  return &(circuit_->block_list);
}

std::vector<Net> *Placer::NetList() {
  return &(circuit_->net_list);
}

bool Placer::IsBoundaryProper() {
  if (circuit_->MaxWidth() > Right() - Left()) {
    std::cout << "Improper boundary: MaxWidth() > Right() - Left()\n";
    return false;
  }
  if (circuit_->MaxHeight() > Top() - Bottom()) {
    std::cout << "Improper boundary: MaxHeight() > Top() - Bottom()\n";
    return false;
  }

  return true;
}

void Placer::SetBoundaryAuto() {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  int tot_block_area = circuit_->TotArea();
  int width = std::ceil(std::sqrt(tot_block_area/aspect_ratio_/filling_rate_));
  int height = std::ceil(width * aspect_ratio_);
  std::cout << "Pre-set aspect ratio: " << aspect_ratio_ << "\n";
  aspect_ratio_ = height/(double)width;
  std::cout << "Adjusted aspect rate: " << aspect_ratio_ << "\n";
  left_ = (int)(circuit_->AveWidth());
  right_ = left_ + width;
  bottom_ = (int)(circuit_->AveWidth());
  top_ = bottom_ + height;
  int area = height * width;
  std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  filling_rate_ = tot_block_area/(double)area;
  std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
  Assert(circuit_ != nullptr, "Must set input circuit before setting boundaries");
  Assert(left < right, "Invalid boundary setting: left boundary should be less than right boundary!");
  Assert(bottom < top, "Invalid boundary setting: bottom boundary should be less than top boundary!");
  int tot_block_area = circuit_->TotArea();
  int tot_area = (right - left) * (top - bottom);
  Assert(tot_area >= tot_block_area, "Invalid boundary setting: given region has smaller area than total block area!");
  std::cout << "Pre-set filling rate: " << filling_rate_ << "\n";
  filling_rate_ = tot_block_area/(double)tot_area;
  std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
  left_ = left;
  right_ = right;
  bottom_ = bottom;
  top_ = top;
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundaryDef() {
  left_ = GetCircuit()->def_left;
  right_ = GetCircuit()->def_right;
  bottom_ = GetCircuit()->def_bottom;
  top_ = GetCircuit()->def_top;
  Assert(IsBoundaryProper(), "Invalid boundary setting");
}

int Placer::Left() {
  return left_;
}

int Placer::Right() {
  return right_;
}

int Placer::Bottom() {
  return bottom_;
}

int Placer::Top() {
  return top_;
}

void Placer::ReportBoundaries() {
  std::cout << "Left, Right, Bottom, Top:\n";
  std::cout << "  " << Left() << ", " << Right() << ", " << Bottom() << ", " << Top() << "\n";
}

bool Placer::UpdateAspectRatio() {
  if ((right_ - left_ == 0) || (top_ - bottom_ == 0)) {
    std::cout << "Error!\n";
    std::cout << "Zero Height or Width of placement region!\n";
    ReportBoundaries();
    return false;
  }
  aspect_ratio_ = (top_ - bottom_)/(double)(right_ - left_);
  return true;
}

void Placer::NetSortBlkPin() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLX!");
  GetCircuit()->NetSortBlkPin();
}

double Placer::HPWLX() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLX!");
  return GetCircuit()->HPWLX();
}

double Placer::HPWLY() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLY!");
  return GetCircuit()->HPWLY();
}

double Placer::HPWL() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
  return GetCircuit()->HPWL();
}

void Placer::ReportHPWL() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWL!");
  GetCircuit()->ReportHPWL();
}

void Placer::ReportHPWLCtoC() {
  Assert(circuit_ != nullptr, "No input circuit specified, cannot compute HPWLCtoC!");
  GetCircuit()->ReportHPWLCtoC();
}

void Placer::TakeOver(Placer *placer) {
  aspect_ratio_ = placer->AspectRatio();
  filling_rate_ = placer->FillingRate();
  left_ = placer->Left();
  right_ = placer->Right();
  bottom_ = placer->Bottom();
  top_ = placer->Top();
  circuit_ = placer->GetCircuit();
}

void Placer::SanityCheck() {
  double epsilon = 1e-3;
  Assert(filling_rate_ > epsilon, "Filling rate should be in a proper range, for example [0.1, 1], current value: " + std::to_string(filling_rate_));
}

void Placer::GenMATLABScript(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "\n";
  for (auto &&block: circuit_->block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  ost.close();
}

bool Placer::SaveNodeTerminal(std::string const &terminal_file, std::string const &node_file) {
  std::ofstream ost(terminal_file.c_str());
  std::ofstream ost1(node_file.c_str());
  Assert(ost.is_open() && ost1.is_open(), "Cannot open file " + terminal_file + " or " + node_file);
  for (auto &&block: circuit_->block_list) {
    if (block.IsMovable()) {
      ost1 << block.X() << "\t" << block.Y() << "\n";
    }
    else {
      double low_x, low_y, width, height;
      width = block.Width();
      height = block.Height();
      low_x = block.LLX();
      low_y = block.LLY();
      for (int j=0; j<height; j++) {
        ost << low_x << "\t" << low_y+j << "\n";
        ost << low_x+width << "\t" << low_y+j << "\n";
      }
      for (int j=0; j<width; j++) {
        ost << low_x+j << "\t" << low_y << "\n";
        ost << low_x+j << "\t" << low_y+height << "\n";
      }
    }
  }
  ost.close();
  ost1.close();
  return true;
}

bool Placer::SaveDEFFile(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  // 1. print file header?
  ost << "VERSION 5.8 ;\n"
      << "DIVIDERCHAR \"/\" ;\n"
      << "BUSBITCHARS \"[]\" ;\n";
  ost << "DESIGN tmp_circuit_name\n";
  ost << "UNITS DISTANCE MICRONS 2000 \n\n"; // temporary values, depends on LEF file

  // no core rows?
  // no tracks?

  // 2. print component
  std::cout << circuit_->block_list.size() << "\n";
  ost << "COMPONENTS " << circuit_->block_list.size() << " ;\n";
  for (auto &&block: circuit_->block_list) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED"
        << " ("
        << " " + std::to_string((int)(block.LLX()*circuit_->def_distance_microns*circuit_->m2_pitch))
        << " " + std::to_string((int)(block.LLY()*circuit_->def_distance_microns*circuit_->m2_pitch))
        << " ) "
        << block.OrientStr() + " ;\n";
  }
  ost << "END COMPONENTS\n";

  // 3. print net
  ost << "NETS " << circuit_->net_list.size() << " ;\n";
  for (auto &&net: circuit_->net_list) {
    ost << "- "
        << *(net.Name()) << "\n";
    ost << " ";
    for (auto &&pin_pair: net.blk_pin_list) {
      ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
  return true;
}
