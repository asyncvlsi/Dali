//
// Created by Yihang Yang on 11/28/19.
//

#include "layer.h"

MetalLayer::MetalLayer(std::pair<const std::string, int> *name_num_ptr) :
    Layer(),
    name_num_ptr_(name_num_ptr),
    area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(HORIZONTAL) {}

MetalLayer::MetalLayer(double width,
                       double spacing,
                       std::pair<const std::string, int> *name_num_ptr,
                       MetalDirection direction) :
    Layer(width, spacing),
    name_num_ptr_(name_num_ptr),
    area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(direction) {}

void MetalLayer::Report() {
  std::cout << "  MetalLayer Name: " << *Name() << "\n"
            << "    Assigned Num: " << Num() << "\n"
            << "    Width and Spacing: " << Width() << " " << Spacing() << "\n"
            << "    MinArea: " << Area() << "\n"
            << "    Direction: " << MetalDirectionStr(Direction()) << "\n"
            << "    Pitch: " << PitchX() << "  " << PitchY() << "\n";
}

void WellLayer::SetParams(double width, double spacing, double op_spacing, double max_plug_dist) {
  SetWidth(width);
  SetSpacing(spacing);
  SetOpSpacing(op_spacing);
  SetMaxPlugDist(max_plug_dist);
}

void WellLayer::Report() {
  std::cout << "    Width:       " << Width() << " um\n"
            << "    Spacing:     " << Spacing() << " um\n"
            << "    OpSpacing:   " << OpSpacing() << " um\n"
            << "    MaxPlugDist: " << MaxPlugDist() << " um\n";
}
