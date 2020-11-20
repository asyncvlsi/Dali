//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_SRC_CIRCUIT_IOPIN_H_
#define DALI_SRC_CIRCUIT_IOPIN_H_

#include <string>

#include "common/logging.h"
#include "common/misc.h"
#include "net.h"
#include "layer.h"
#include "status.h"

class Net;

class IOPin {
 private:
  std::pair<const std::string, int> *name_num_pair_ptr_;
  Net *net_ptr_;
  SignalDirection direction_;
  SignalUse use_;
  MetalLayer *layer_ptr_;
  RectD rect_;
  PlaceStatus init_place_status_;
  PlaceStatus place_status_;
  double lx_, ly_;
  BlockOrient orient_;
 public:
  explicit IOPin(std::pair<const std::string, int> *name_num_pair_ptr);
  IOPin(std::pair<const std::string, int> *name_num_pair_ptr, double lx, double ly);
  IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
        SignalDirection direction,
        PlaceStatus init_place_status,
        double lx,
        double ly);

  const std::string *Name() const {return &(name_num_pair_ptr_->first);}
  int Num() const {return name_num_pair_ptr_->second;}
  Net *GetNet() const {return net_ptr_;}
  SignalDirection SigDirection() const {return direction_;}
  SignalUse SigUse() const {return  use_;}
  MetalLayer *Layer() const {return layer_ptr_;}
  RectD *GetRect() {return &rect_;}
  bool IsPlaced() const {return place_status_ == FIXED_ || place_status_ == PLACED_;}
  bool IsPrePlaced() const{return init_place_status_ == FIXED_ || init_place_status_ == PLACED_;}
  double X() const {return lx_;}
  double Y() const {return ly_;}

  void SetNet(Net *net_ptr) {
    Assert(net_ptr != nullptr, "Cannot set @param net_ptr to nullptr in function: IOPin::SetNet()");
    net_ptr_ = net_ptr;
  }
  void SetDirection(SignalDirection direction) {direction_ = direction;}
  void SetUse(SignalUse use) { use_ = use; }
  void SetLayer(MetalLayer *layer_ptr){
    Assert(layer_ptr != nullptr, "Cannot set @param layer_ptr to nullptr in function: IOPin::SetLayer()");
    layer_ptr_ = layer_ptr;
  }
  void SetRect(double llx, double lly, double urx, double ury) {
    rect_.SetValue(llx, lly, urx, ury);
  }
  void SetLoc(double lx, double ly, PlaceStatus place_status = PLACED_) {
    lx_ = lx;
    ly_ = ly;
    place_status_ = place_status;
  }

  void Report() const;
};

#endif //DALI_SRC_CIRCUIT_IOPIN_H_
