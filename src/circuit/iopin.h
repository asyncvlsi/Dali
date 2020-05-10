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

class IOPin {
 private:
  std::pair<const std::string, int> *name_num_pair_;
  Net *net_;
  SignalDirection direction_;
  MetalLayer *layer_;
  RectD rect_;
  PlaceStatus place_status_;
  double lx_, ly_;
 public:
  explicit IOPin(std::pair<const std::string, int> *name_num_pair);
  IOPin(std::pair<const std::string, int> *name_num_pair, double lx, double ly);
  IOPin(std::pair<const std::string, int> *name_num_pair,
        SignalDirection direction,
        PlaceStatus place_status,
        double lx,
        double ly);

  const std::string *Name() const;
  int Num() const;
  Net *GetNet() const;
  SignalDirection Direction() const;
  MetalLayer *Layer() const;
  RectD *GetRect();
  bool IsPlaced() const;
  double X() const;
  double Y() const;

  void SetNet(Net *net);
  void SetDirection(SignalDirection direction);
  void SetLayer(MetalLayer *layer);
  void SetRect(double llx, double lly, double urx, double ury);
  void SetLoc(double lx, double ly, PlaceStatus place_status = PLACED);

  void Report() const;
};

inline const std::string *IOPin::Name() const {
  return &(name_num_pair_->first);
}

inline int IOPin::Num() const {
  return name_num_pair_->second;
}

inline Net *IOPin::GetNet() const {
  return net_;
}

inline SignalDirection IOPin::Direction() const {
  return direction_;
}

inline MetalLayer *IOPin::Layer() const {
  return layer_;
}

inline RectD *IOPin::GetRect() {
  return &rect_;
}

inline bool IOPin::IsPlaced() const {
  return place_status_ == FIXED || place_status_ == PLACED;
}

inline double IOPin::X() const {
  return lx_;
}

inline double IOPin::Y() const {
  return ly_;
}

inline void IOPin::SetNet(Net *net) {
  Assert(net != nullptr, "Cannot set attribute Net *net to nullptr");
  net_ = net;
}
inline void IOPin::SetDirection(SignalDirection direction) {
  direction_ = direction;
}

inline void IOPin::SetLayer(MetalLayer *layer) {
  Assert(layer != nullptr, "Cannot set attribute MetalLayer *layer to nullptr");
  layer_ = layer;
}

inline void IOPin::SetRect(double llx, double lly, double urx, double ury) {
  rect_.SetValue(llx, lly, urx, ury);
}

inline void IOPin::SetLoc(double lx, double ly, PlaceStatus place_status) {
  lx_ = lx;
  ly_ = ly;
  place_status_ = place_status;
}

#endif //DALI_SRC_CIRCUIT_IOPIN_H_
