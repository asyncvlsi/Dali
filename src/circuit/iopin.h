//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_SRC_CIRCUIT_IOPIN_H_
#define DALI_SRC_CIRCUIT_IOPIN_H_

#include <string>

#include "common/misc.h"
#include "net.h"
#include "layer.h"
#include "status.h"

class Net;

class IOPin {
 private:
  std::pair<const std::string, int> *name_num_pair_;
  Net *net_;
  SignalDirection direction_;
  SignalUse use_;
  MetalLayer *layer_;
  RectD rect_;
  PlaceStatus init_place_status_;
  PlaceStatus place_status_;
  double lx_, ly_;
  BlockOrient orient_;
 public:
  explicit IOPin(std::pair<const std::string, int> *name_num_pair);
  IOPin(std::pair<const std::string, int> *name_num_pair, double lx, double ly);
  IOPin(std::pair<const std::string, int> *name_num_pair,
        SignalDirection direction,
        PlaceStatus init_place_status,
        double lx,
        double ly);

  const std::string *Name() const {return &(name_num_pair_->first);}
  int Num() const {return name_num_pair_->second;}
  Net *GetNet() const {return net_;}
  SignalDirection SigDirection() const {return direction_;}
  SignalUse SigUse() const {return  use_;}
  MetalLayer *Layer() const {return layer_;}
  RectD *GetRect() {return &rect_;}
  bool IsPlaced() const {return place_status_ == FIXED_ || place_status_ == PLACED_;}
  bool IsPrePlaced() const{return init_place_status_ == FIXED_ || init_place_status_ == PLACED_;}
  double X() const {return lx_;}
  double Y() const {return ly_;}

  void SetNet(Net *net) {
    Assert(net != nullptr, "Cannot set attribute Net *net to nullptr");
    net_ = net;
  }
  void SetDirection(SignalDirection direction) {direction_ = direction;}
  void SetUse(SignalUse use) { use_ = use; }
  void SetLayer(MetalLayer *layer){
    Assert(layer != nullptr, "Cannot set attribute MetalLayer *layer to nullptr");
    layer_ = layer;
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
