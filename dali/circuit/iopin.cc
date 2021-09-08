//
// Created by Yihang Yang on 11/28/19.
//

#include "iopin.h"

namespace dali {

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(INPUT),
    use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(UNPLACED),
    place_status_(UNPLACED),
    loc_x_(0),
    loc_y_(0),
    orient_(N) {
    rect_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
    rect_[0].SetValue(0, 0, 0, 0);
}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
             double loc_x,
             double loc_y) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(INPUT),
    use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(PLACED),
    place_status_(PLACED),
    loc_x_(loc_x),
    loc_y_(loc_y),
    orient_(N) {
    rect_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
    rect_[0].SetValue(0, 0, 0, 0);
}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
             SignalDirection direction,
             PlaceStatus init_place_status,
             double loc_x,
             double loc_y) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(direction),
    use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(init_place_status),
    place_status_(init_place_status),
    loc_x_(loc_x),
    loc_y_(loc_y),
    orient_(N) {
    rect_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
    rect_[0].SetValue(0, 0, 0, 0);
}

const std::string *IOPin::Name() const {
    return &(name_num_pair_ptr_->first);
}

int IOPin::Num() const {
    return name_num_pair_ptr_->second;
}

Net *IOPin::GetNet() const {
    return net_ptr_;
}

SignalDirection IOPin::SigDirection() const {
    return direction_;
}

SignalUse IOPin::SigUse() const {
    return use_;
}

MetalLayer *IOPin::Layer() const {
    return layer_ptr_;
}

RectD *IOPin::GetRect() {
    return &rect_[0];
}

bool IOPin::IsPlaced() const {
    return place_status_ == FIXED || place_status_ == PLACED;
}

bool IOPin::IsPrePlaced() const {
    return init_place_status_ == FIXED || init_place_status_ == PLACED;
}
double IOPin::X() const {
    return loc_x_;
}

double IOPin::Y() const {
    return loc_y_;
}

PlaceStatus IOPin::GetPlaceStatus() const {
    return place_status_;
}

BlockOrient IOPin::GetOrient() const {
    return orient_;
}

void IOPin::SetNet(Net *net_ptr) {
    DaliExpects(net_ptr != nullptr,
                "Cannot set @param net_ptr to nullptr in function: IOPin::SetNet()");
    net_ptr_ = net_ptr;
}
void IOPin::SetDirection(SignalDirection direction) {
    direction_ = direction;
}

void IOPin::SetUse(SignalUse use) {
    use_ = use;
}

void IOPin::SetLayer(MetalLayer *layer_ptr) {
    DaliExpects(layer_ptr != nullptr,
                "Cannot set @param layer_ptr to nullptr in function: IOPin::SetLayer()");
    layer_ptr_ = layer_ptr;
}

void IOPin::SetRect(double llx, double lly, double urx, double ury) {
    /****
     * rotate 0 degree
     *    llx' = llx;
     *    lly' = lly;
     *    urx' = urx;
     *    ury' = ury;
     ****/
    rect_[N - N].SetValue(llx, lly, urx, ury);

    /****
     * rotate 180 degree counterclockwise
     *    llx' = -urx;
     *    lly' = -ury;
     *    urx' = -llx;
     *    ury' = -lly;
    ****/
    rect_[S - N].SetValue(-urx, -ury, -llx, -lly);

    /****
     * rotate 90 degree counterclockwise
     *    llx' = -ury;
     *    lly' =  llx;
     *    urx' = -lly;
     *    ury' =  urx;
     * ****/
    rect_[W - N].SetValue(-ury, llx, -lly, urx);

    /****
     * rotate 270 degree counterclockwise
     *    llx' =  lly;
     *    lly' = -urx;
     *    urx' =  ury;
     *    ury' = -llx;
     * ****/
    rect_[E - N].SetValue(lly, -urx, ury, -llx);

    /****
     * rotate 0 degree counterclockwise
     *    llx' = llx;
     *    lly' = lly;
     *    urx' = urx;
     *    ury' = ury;
     * flip along the y-direction through the location of this pin
     *    llx' = -urx;
     *    lly' =  lly;
     *    urx' = -llx;
     *    ury' =  ury;
     * ****/
    rect_[FN - N].SetValue(-urx, lly, -llx, ury);

    /****
     * rotate 180 degree counterclockwise
     *    llx' = -urx;
     *    lly' = -ury;
     *    urx' = -llx;
     *    ury' = -lly;
     * flip along the y-direction through the location of this pin
     *    llx' =  llx;
     *    lly' = -ury;
     *    urx' =  urx;
     *    ury' = -lly;
     * ****/
    rect_[FS - N].SetValue(llx, -ury, urx, -lly);

    /****
     * rotate 90 degree counterclockwise
     *    llx' = -ury;
     *    lly' =  llx;
     *    urx' = -lly;
     *    ury' =  urx;
     * flip along the y-direction through the location of this pin
     *    llx' =  lly;
     *    lly' =  llx;
     *    urx' =  ury;
     *    ury' =  urx;
     * ****/
    rect_[FW - N].SetValue(lly, llx, ury, urx);

    /****
     * rotate 270 degree counterclockwise
     *    llx' =  lly;
     *    lly' = -urx;
     *    urx' =  ury;
     *    ury' = -llx;
     * flip along the y-direction through the location of this pin
     *    llx' = -ury;
     *    lly' = -urx;
     *    urx' = -lly;
     *    ury' = -llx;
     * ****/
    rect_[FE - N].SetValue(-ury, -urx, -lly, -llx);
}

void IOPin::SetInitPlaceStatus(PlaceStatus init_place_status) {
    init_place_status_ = init_place_status;
}

void IOPin::SetLocX(double loc_x) {
    loc_x_ = loc_x;
}

void IOPin::SetLocY(double loc_y) {
    loc_y_ = loc_y;
}

void IOPin::SetPlaceStatus(PlaceStatus place_status) {
    place_status_ = place_status;
}

void IOPin::SetLoc(double loc_x,
                   double loc_y,
                   PlaceStatus place_status) {
    loc_x_ = loc_x;
    loc_y_ = loc_y;
    place_status_ = place_status;
}

void IOPin::SetOrient(BlockOrient orient) {
    orient_ = orient;
}

void IOPin::SetShape(double llx, double lly, double urx, double ury) {
    is_shape_set_ = true;
    SetRect(llx, lly, urx, ury);
}

double IOPin::LX(double spacing) const {
    return loc_x_ + rect_[orient_].LLX() - spacing;
}

double IOPin::UX(double spacing) const {
    return loc_x_ + rect_[orient_].URX() + spacing;
}

double IOPin::LY(double spacing) const {
    return loc_y_ + rect_[orient_].LLY() - spacing;
}

double IOPin::UY(double spacing) const {
    return loc_y_ + rect_[orient_].URY() + spacing;
}

bool IOPin::IsShapeSet() const {
    return is_shape_set_;
}

void IOPin::Report() const {
    std::string net_name = (net_ptr_ == nullptr) ? "NA" : *(net_ptr_->Name());
    BOOST_LOG_TRIVIAL(info) << "  I/O PIN name: " << *Name() << "\n"
                            << "    Net connected: " << net_name << "\n";
}

}
