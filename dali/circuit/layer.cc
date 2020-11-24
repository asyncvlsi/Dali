//
// Created by Yihang Yang on 11/28/19.
//

#include "layer.h"

namespace dali {

Layer::Layer() :
    width_(0),
    spacing_(0) {}

Layer::Layer(double width, double spacing) :
    width_(width),
    spacing_(spacing) {
  DaliExpects(width >= 0 && spacing >= 0, "Negative width or spacing?\n");
}

MetalLayer::MetalLayer(std::pair<const std::string, int> *name_num_pair_ptr) :
    Layer(),
    name_num_pair_ptr_(name_num_pair_ptr),
    min_area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(HORIZONTAL_) {}

MetalLayer::MetalLayer(double width,
                       double spacing,
                       std::pair<const std::string, int> *name_num_pair_ptr,
                       MetalDirection direction) :
    Layer(width, spacing),
    name_num_pair_ptr_(name_num_pair_ptr),
    min_area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(direction) {}

void MetalLayer::Report() const {
  BOOST_LOG_TRIVIAL(info) << "  MetalLayer Name: " << *Name() << "\n"
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
  DaliExpects(op_spacing >= 0, "Negative opposite spacing?");
  DaliExpects(max_plug_dist_ >= 0, "Negative maximum plug distance?");
}

void WellLayer::SetParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
  SetWidth(width);
  SetSpacing(spacing);
  SetOpSpacing(op_spacing);
  SetMaxPlugDist(max_plug_dist);
  SetOverhang(overhang);
}

void WellLayer::Report() {
  BOOST_LOG_TRIVIAL(info) << "    Width:       " << Width() << " um\n"
                          << "    Spacing:     " << Spacing() << " um\n"
                          << "    OpSpacing:   " << OpSpacing() << " um\n"
                          << "    MaxPlugDist: " << MaxPlugDist() << " um\n"
                          << "    Overhang:    " << Overhang() << "um\n";
}

}
