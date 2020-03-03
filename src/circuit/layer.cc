//
// Created by Yihang Yang on 11/28/19.
//

#include "layer.h"

Layer::Layer() :
    width_(0),
    spacing_(0) {}

Layer::Layer(double width, double spacing) :
    width_(width),
    spacing_(spacing) {}

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

WellLayer::WellLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) :
    Layer(width, spacing),
    op_spacing_(op_spacing),
    max_plug_dist_(max_plug_dist),
    overhang_(overhang) {
  Assert(op_spacing >= 0, "Negative opposite spacing?");
  Assert(max_plug_dist_ >= 0, "Negative maximum plug distance?");
}

void WellLayer::SetParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
  SetWidth(width);
  SetSpacing(spacing);
  SetOpSpacing(op_spacing);
  SetMaxPlugDist(max_plug_dist);
  SetOverhang(overhang);
}

void WellLayer::Report() {
  std::cout << "    Width:       " << Width() << " um\n"
            << "    Spacing:     " << Spacing() << " um\n"
            << "    OpSpacing:   " << OpSpacing() << " um\n"
            << "    MaxPlugDist: " << MaxPlugDist() << " um\n"
            << "    Overhang:    " << Overhang() << "um\n";
}
