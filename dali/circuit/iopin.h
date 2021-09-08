//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_SRC_CIRCUIT_IOPIN_H_
#define DALI_SRC_CIRCUIT_IOPIN_H_

#include <string>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "net.h"
#include "layer.h"
#include "status.h"

namespace dali {

class Net;

class IOPin {
  private:
    std::pair<const std::string, int> *name_num_pair_ptr_;
    Net *net_ptr_;
    SignalDirection direction_;
    SignalUse use_;
    MetalLayer *layer_ptr_;
    std::vector<RectD> rect_; // the pin geometry when the orientation is N
    bool is_shape_set_ = false;
    PlaceStatus init_place_status_;
    PlaceStatus place_status_;
    double loc_x_, loc_y_;
    BlockOrient orient_;
  public:
    explicit IOPin(std::pair<const std::string, int> *name_num_pair_ptr);
    IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
          double loc_x,
          double loc_y);
    IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
          SignalDirection direction,
          PlaceStatus init_place_status,
          double loc_x,
          double loc_y);

    const std::string *Name() const;
    int Num() const;
    Net *GetNet() const;
    SignalDirection SigDirection() const;
    SignalUse SigUse() const;
    MetalLayer *Layer() const;
    RectD *GetRect();
    bool IsPlaced() const;
    bool IsPrePlaced() const;
    double X() const;
    double Y() const;
    PlaceStatus GetPlaceStatus() const;
    BlockOrient GetOrient() const;

    void SetNet(Net *net_ptr);
    void SetDirection(SignalDirection direction);
    void SetUse(SignalUse use);
    void SetLayer(MetalLayer *layer_ptr);
    void SetRect(double llx, double lly, double urx, double ury);
    void SetInitPlaceStatus(PlaceStatus init_place_status);
    void SetLocX(double loc_x);
    void SetLocY(double loc_y);
    void SetPlaceStatus(PlaceStatus place_status);
    void SetLoc(double loc_x,
                double loc_y,
                PlaceStatus place_status = PLACED);
    void SetOrient(BlockOrient orient);
    void SetShape(double llx, double lly, double urx, double ury);

    double LX(double spacing = 0) const;
    double UX(double spacing = 0) const;
    double LY(double spacing = 0) const;
    double UY(double spacing = 0) const;
    bool IsShapeSet() const;

    void Report() const;
};

}

#endif //DALI_SRC_CIRCUIT_IOPIN_H_
