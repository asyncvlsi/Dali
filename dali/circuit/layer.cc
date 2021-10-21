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
    DaliExpects(width >= 0 && spacing >= 0, "Negative width or spacing?");
}

void Layer::SetWidth(double width) {
    DaliExpects(width >= 0, "Negative width not allowed");
    width_ = width;
}

void Layer::SetSpacing(double spacing) {
    DaliExpects(spacing >= 0, "Negative spacing not allowed");
    spacing_ = spacing;
}

MetalLayer::MetalLayer(
    std::pair<const std::string, int> *name_id_pair_ptr
) : Layer(),
    name_id_pair_ptr_(name_id_pair_ptr),
    min_area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(HORIZONTAL) {}

MetalLayer::MetalLayer(
    double width,
    double spacing,
    std::pair<const std::string, int> *name_id_pair_ptr,
    MetalDirection direction
) : Layer(width, spacing),
    name_id_pair_ptr_(name_id_pair_ptr),
    min_area_(0),
    x_pitch_(0),
    y_pitch_(0),
    direction_(direction) {}

const std::string &MetalLayer::Name() const {
    return name_id_pair_ptr_->first;
}

int MetalLayer::Id() const {
    return name_id_pair_ptr_->second;
}

void MetalLayer::SetMinArea(double min_area) {
    DaliExpects(min_area >= 0, "Negative minarea?");
    min_area_ = min_area;
}

double MetalLayer::MinArea() const {
    return min_area_;
}

double MetalLayer::MinHeight() const {
    return min_area_ / width_;
}

void MetalLayer::SetPitch(double x_pitch, double y_pitch) {
    DaliExpects(x_pitch >= 0 && y_pitch >= 0, "Negative metal pitch?");
    x_pitch_ = x_pitch;
    y_pitch_ = y_pitch;
}

void MetalLayer::SetPitchUsingWidthSpacing() {
    double x_pitch = width_ + spacing_;
    double y_pitch = width_ + spacing_;
    SetPitch(x_pitch, y_pitch);
}

double MetalLayer::PitchX() const {
    return x_pitch_;
}

double MetalLayer::PitchY() const {
    return y_pitch_;
}

void MetalLayer::SetDirection(MetalDirection direction) {
    direction_ = direction;
}

MetalDirection MetalLayer::Direction() const {
    return direction_;
}

std::string MetalLayer::DirectionStr() const {
    return MetalDirectionStr(direction_);
}

void MetalLayer::Report() const {
    BOOST_LOG_TRIVIAL(info)
        << "  MetalLayer Name: " << Name() << "\n"
        << "    Assigned Num: " << Id() << "\n"
        << "    Width and Spacing: " << Width() << " "
        << Spacing() << "\n"
        << "    MinArea: " << MinArea() << "\n"
        << "    Direction: "
        << DirectionStr() << "\n"
        << "    Pitch: " << PitchX() << "  " << PitchY()
        << "\n";
}

WellLayer::WellLayer(
    double width,
    double spacing,
    double opposite_spacing,
    double max_plug_dist,
    double overhang
) : Layer(width, spacing),
    opposite_spacing_(opposite_spacing),
    max_plug_dist_(max_plug_dist),
    overhang_(overhang) {
    DaliExpects(opposite_spacing >= 0, "Negative opposite spacing?");
    DaliExpects(max_plug_dist >= 0, "Negative maximum plug distance?");
}

void WellLayer::SetParams(
    double width,
    double spacing,
    double opposite_spacing,
    double max_plug_dist,
    double overhang
) {
    SetWidth(width);
    SetSpacing(spacing);
    SetOppositeSpacing(opposite_spacing);
    SetMaxPlugDist(max_plug_dist);
    SetOverhang(overhang);
}

void WellLayer::SetOppositeSpacing(double opposite_spacing) {
    DaliExpects(opposite_spacing >= 0,
                "Negative opposite spacing not allowed");
    opposite_spacing_ = opposite_spacing;
}

double WellLayer::OppositeSpacing() const {
    return opposite_spacing_;
}

void WellLayer::SetMaxPlugDist(double max_plug_dist) {
    DaliExpects(max_plug_dist >= 0,
                "Negative max plug distance not allowed");
    max_plug_dist_ = max_plug_dist;
}

double WellLayer::MaxPlugDist() const {
    return max_plug_dist_;
}

void WellLayer::SetOverhang(double overhang) {
    DaliExpects(overhang >= 0,
                "Negative well/diffusion overhang not allowed");
    overhang_ = overhang;
}

double WellLayer::Overhang() const {
    return overhang_;
}

void WellLayer::Report() {
    BOOST_LOG_TRIVIAL(info)
        << "    Width:       " << Width() << " um\n"
        << "    Spacing:     " << Spacing() << " um\n"
        << "    OppositeSpacing:   " << OppositeSpacing() << " um\n"
        << "    MaxPlugDist: " << MaxPlugDist() << " um\n"
        << "    Overhang:    " << Overhang() << "um\n";
}

}
