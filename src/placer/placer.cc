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

Placer::Placer(double aspectRatio, double fillingRate) : aspect_ratio_(aspectRatio), filling_rate_(fillingRate) {
  left_ = 0;
  right_ = 0;
  bottom_ = 0;
  top_ = 0;
  circuit_ = nullptr;
}

void Placer::SetInputCircuit(Circuit *circuit) {
  Assert(circuit != nullptr, "Invalid input circuit: not allowed to set nullptr as the input!");
  Assert(circuit->block_list.empty(), "Invalid input circuit: empty block list!");
  Assert(circuit->net_list.empty(), "Invalid input circuit: empty net list!");
  circuit_ = circuit;
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
  std::cout << "\t" << Left() << ", " << Right() << ", " << Bottom() << ", " << Top() << "\n";
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

bool Placer::GenMATLABScript(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: circuit_->block_list) {
    ost << "rectangle('Position',[" << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "], 'LineWidth', 1, 'EdgeColor','blue')\n";
  }
  for (auto &&net: circuit_->net_list) {
    for (size_t i=0; i<net.blk_pin_pair_list.size(); i++) {
      for (size_t j=i+1; j<net.blk_pin_pair_list.size(); j++) {
        ost << "line([" << net.blk_pin_pair_list[i].AbsX() << "," << net.blk_pin_pair_list[j].AbsX() << "],[" << net.blk_pin_pair_list[i].AbsY() << "," << net.blk_pin_pair_list[j].AbsY() << "],'lineWidth', 0.5)\n";
      }
    }
  }
  ost << "rectangle('Position',[" << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool Placer::SaveNodeTerminal(std::string const &NameOfFile, std::string const &NameOfFile1) {
  std::ofstream ost(NameOfFile.c_str());
  std::ofstream ost1(NameOfFile1.c_str());
  if ((ost.is_open()==0)||(ost1.is_open()==0)) {
    std::cout << "Cannot open file " << NameOfFile << " or " << NameOfFile1 <<  "\n";
    return false;
  }
  for (auto &&node: circuit_->block_list) {
    if (node.IsMovable()) {
      ost1 << node.X() << "\t" << node.Y() << "\n";
    }
    else {
      double low_x, low_y, width, height;
      width = node.Width();
      height = node.Height();
      low_x = node.LLX();
      low_y = node.LLY();
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
    for (auto &&pin_pair: net.blk_pin_pair_list) {
      ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
  return true;
}

Placer::~Placer() {

}

