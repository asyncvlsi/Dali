//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_IOPIN_H_
#define HPCC_SRC_CIRCUIT_IOPIN_H_

#include <string>
#include "../common/misc.h"
#include "net.h"
#include "layer.h"
#include "status.h"
#include "rect.h"

class IOPin {
 private:
  std::pair<const std::string, int>* name_num_pair_;
  Net *net_;
  SignalDirection direction_;
  MetalLayer *layer_;
  Rect rect_;
  PlaceStatus place_status_;
  double lx_, ly_;
 public:
  explicit IOPin(std::pair<const std::string, int>* name_num_pair);
  IOPin(std::pair<const std::string, int>* name_num_pair, double lx, double ly);
  IOPin(std::pair<const std::string, int>* name_num_pair, SignalDirection direction, PlaceStatus place_status, double lx, double ly);

  const std::string *Name() const {return &(name_num_pair_->first);}
  int Num() const {return name_num_pair_->second;}
  Net *GetNet() const {return net_;}
  SignalDirection Direction() const {return direction_;}
  MetalLayer *Layer() const {return layer_;}
  Rect *GetRect() {return &rect_;}
  double X() {return lx_;}
  double Y() {return ly_;}

  void SetNet(Net *net) {Assert(net!= nullptr, "Cannot set attribute Net *net to nullptr"); net_ = net;}
  void SetDirection(SignalDirection direction) {direction_ = direction;}
  void SetLayer(MetalLayer *layer) {Assert(layer!= nullptr, "Cannot set attribute MetalLayer *layer to nullptr"); layer_ = layer;}
  void SetRect(double llx, double lly, double urx, double ury) {rect_.SetValue(llx, lly, urx, ury);}
  void SetLoc(double lx, double ly, PlaceStatus place_status=FIXED) {lx_=lx; ly_=ly; place_status_=place_status;}

  void Report() const;
};

#endif //HPCC_SRC_CIRCUIT_IOPIN_H_
