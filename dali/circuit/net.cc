/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "net.h"

#include <algorithm>
#include <cfloat>

#include "dali/common/misc.h"

namespace dali {

Net::Net(std::pair<const std::string, int>* name_id_pair_ptr, size_t capacity,
         double weight)
    : name_id_pair_ptr_(name_id_pair_ptr), weight_(weight) {
  cnt_fixed_ = 0;
  max_x_pin_id_ = -1;
  min_x_pin_id_ = -1;
  max_y_pin_id_ = -1;
  min_y_pin_id_ = -1;
  inv_p_ = 0;
  aux_ptr_ = nullptr;
  blk_pins_.reserve(capacity);
}

const std::string& Net::Name() const { return name_id_pair_ptr_->first; }

int Net::Id() const { return name_id_pair_ptr_->second; }

void Net::AddComponentPinPair(Component* component_ptr, Pin* pin_ptr) {
  if (blk_pins_.size() < blk_pins_.capacity()) {
    blk_pins_.emplace_back(component_ptr, pin_ptr);
    if (!(pin_ptr->IsInput())) driver_pin_index = int(blk_pins_.size()) - 1;
    if (!component_ptr->IsMovable()) {
      ++cnt_fixed_;
    }
    // because net list is stored as a vector, so the location of a net will
    // change, thus here, we have to use Num() to find a net, although a pointer
    // to this net is more convenient.
    component_ptr->NetList().push_back(Id());
    int p_minus_one = int(blk_pins_.size()) - 1;
    inv_p_ = p_minus_one > 0 ? 1.0 * weight_ / p_minus_one : 0;
  } else {
    LOG(info) << "Pre-assigned net capacity is full: " << blk_pins_.capacity()
              << ", cannot add more pin to this net:\n";
    LOG(info) << "net name: " << Name() << ", net weight: " << Weight() << "\n";
    for (auto& component_pin_pair : blk_pins_) {
      LOG(info) << "\t" << " (" << component_pin_pair.ComponentName() << " "
                << component_pin_pair.PinName() << ") " << "\n";
    }
    exit(1);
  }
}

std::vector<NetPin>& Net::ComponentPins() { return blk_pins_; }

void Net::AddIoPin(IoPin* io_pin) { iopin_ptrs_.push_back(io_pin); }

std::vector<IoPin*>& Net::IoPinPtrs() { return iopin_ptrs_; }

void Net::SetWeight(double weight) { weight_ = weight; }

double Net::Weight() const { return weight_; }

size_t Net::PinCnt() const { return blk_pins_.size(); }

double Net::InvP() const { return inv_p_; }

void Net::SetAux(NetAux* aux) {
  DaliExpects(aux != nullptr, "Cannot set aux_ptr_ to nullptr");
  aux_ptr_ = aux;
}

NetAux* Net::Aux() { return aux_ptr_; }

void Net::GetXBoundIfBlkAbsent(Component* component_ptr, double& lo,
                               double& hi) {
  lo = -DBL_MAX;
  hi = DBL_MAX;

  int sz = static_cast<int>(blk_pins_.size());
  if (sz == 1) {
    return;
  }

  double max_x = -DBL_MAX;
  double min_x = DBL_MAX;
  double tmp_pin_loc;

  for (auto& pair : blk_pins_) {
    if (pair.ComponentPtr() == component_ptr) continue;
    tmp_pin_loc = pair.AbsX();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
    }
  }

  if (min_x <= max_x) {
    lo = min_x;
    hi = max_x;
  }
}

void Net::GetYBoundIfBlkAbsent(Component* component_ptr, double& lo,
                               double& hi) {
  lo = -DBL_MAX;
  hi = DBL_MAX;

  int sz = static_cast<int>(blk_pins_.size());
  if (sz == 1) {
    return;
  }

  double max_y = -DBL_MAX;
  double min_y = DBL_MAX;
  double tmp_pin_loc;

  for (auto& pair : blk_pins_) {
    if (pair.ComponentPtr() == component_ptr) continue;
    tmp_pin_loc = pair.AbsY();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
    }
  }
  if (min_y <= max_y) {
    lo = min_y;
    hi = max_y;
  }
}

void Net::SortComponentPinList() {
  std::sort(blk_pins_.begin(), blk_pins_.end());
}

/****
 * @brief find the index of the component pin with maximum/minimum x location.
 *
 * When the number of component pins in this net is >= 2, it is possible that
 * all pins have exactly the same location. In this case, we need to choose
 * different indices for max pin and min pin. Otherwise, this special case
 * may potentially lead to some issues although the program may still work.
 *
 * For example, pin0 and pin1 are the only two pins in a net, and they have
 * the same location, we know that when we decompose this net, it only has
 * one edge. However, if pin0 is viewed as both the max_pin and min_pin, then
 * two edges will be created between pin0 and pin1, which is not desired.
 *
 * To prevent this from happening, we will first find the max_x and min_x in
 * this net. If these two locations are different, it is safe. If these two
 * locations are the same, it means all pins in this net have the same location,
 * we can just pick the first two pins as max_pin and min_pin.
 *
 * TODO: or the driver pin as one of the extreme pin.
 */
void Net::UpdateMaxMinIdX() {
  // no pin
  if (blk_pins_.empty()) return;
  // only one pin
  if (blk_pins_.size() == 1) {
    max_x_pin_id_ = 0;
    min_x_pin_id_ = 0;
    return;
  }
  // more than one pin
  max_x_pin_id_ = 0;
  min_x_pin_id_ = 0;
  double max_x = -DBL_MAX;
  double min_x = DBL_MAX;
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; ++i) {
    double tmp_pin_loc = blk_pins_[i].AbsX();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
      max_x_pin_id_ = i;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
      min_x_pin_id_ = i;
    }
  }
  // if maximum x is the same as minimum x, make sure max id is different from
  // min id
  if (max_x == min_x) {
    max_x_pin_id_ = 0;
    min_x_pin_id_ = 1;
  }
}

void Net::UpdateMaxMinIdY() {
  // no pin
  if (blk_pins_.empty()) return;
  // only one pin
  if (blk_pins_.size() == 1) {
    max_y_pin_id_ = 0;
    min_y_pin_id_ = 0;
    return;
  }
  // more than one pin
  max_y_pin_id_ = 0;
  min_y_pin_id_ = 0;
  double max_y = -DBL_MAX;
  double min_y = DBL_MAX;
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; ++i) {
    double tmp_pin_loc = blk_pins_[i].AbsY();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
      max_y_pin_id_ = i;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
      min_y_pin_id_ = i;
    }
  }
  // if maximum y is the same as minimum y, make sure max id is different from
  // min id
  if (max_y == min_y) {
    max_y_pin_id_ = 0;
    min_y_pin_id_ = 1;
  }
}

void Net::UpdateMaxMinIndex() {
  UpdateMaxMinIdX();
  UpdateMaxMinIdY();
}

int Net::MaxComponentPinIdX() const { return max_x_pin_id_; }

int Net::MinComponentPinIdX() const { return min_x_pin_id_; }

int Net::MaxComponentPinIdY() const { return max_y_pin_id_; }

int Net::MinComponentPinIdY() const { return min_y_pin_id_; }

Component* Net::MaxComponentPtrX() const {
  return blk_pins_[max_x_pin_id_].ComponentPtr();
}

Component* Net::MinComponentPtrX() const {
  return blk_pins_[min_x_pin_id_].ComponentPtr();
}

Component* Net::MaxComponentPtrY() const {
  return blk_pins_[max_y_pin_id_].ComponentPtr();
}

Component* Net::MinComponentPtrY() const {
  return blk_pins_[min_y_pin_id_].ComponentPtr();
}

double Net::WeightedHPWLX() {
  if (blk_pins_.size() <= 1) return 0;
  UpdateMaxMinIdX();
  double max_x = blk_pins_[max_x_pin_id_].AbsX();
  double min_x = blk_pins_[min_x_pin_id_].AbsX();
  return (max_x - min_x) * weight_;
}

double Net::WeightedHPWLY() {
  if (blk_pins_.size() <= 1) return 0;
  UpdateMaxMinIdY();
  double max_y = blk_pins_[max_y_pin_id_].AbsY();
  double min_y = blk_pins_[min_y_pin_id_].AbsY();
  return (max_y - min_y) * weight_;
}

double Net::WeightedHPWL() { return WeightedHPWLX() + WeightedHPWLY(); }

double Net::WeightedBboxX() {
  if (blk_pins_.size() <= 1) return 0;
  double max_x = blk_pins_[0].AbsX();
  double min_x = blk_pins_[0].AbsX();
  for (auto& blk_pin : blk_pins_) {
    double center_x = blk_pin.AbsX();
    double half_width = blk_pin.PinPtr()->HalfBboxWidth();
    max_x = std::max(max_x, center_x + half_width);
    min_x = std::min(min_x, center_x - half_width);
  }
  return (max_x - min_x) * weight_;
}

double Net::WeightedBboxY() {
  if (blk_pins_.size() <= 1) return 0;
  double max_y = blk_pins_[0].AbsY();
  double min_y = blk_pins_[0].AbsY();
  for (auto& blk_pin : blk_pins_) {
    double center_y = blk_pin.AbsY();
    double half_height = blk_pin.PinPtr()->HalfBboxHeight();
    max_y = std::max(max_y, center_y + half_height);
    min_y = std::min(min_y, center_y - half_height);
  }
  return (max_y - min_y) * weight_;
}

double Net::WeightedBbox() { return WeightedBboxX() + WeightedBboxY(); }

double Net::MinX() const { return blk_pins_[min_x_pin_id_].AbsX(); }

double Net::MaxX() const { return blk_pins_[max_x_pin_id_].AbsX(); }

double Net::MinY() const { return blk_pins_[min_y_pin_id_].AbsY(); }

double Net::MaxY() const { return blk_pins_[max_y_pin_id_].AbsY(); }

void Net::UpdateMaxMinCtoCX() {
  if (blk_pins_.empty()) return;
  max_x_pin_id_ = 0;
  min_x_pin_id_ = 0;
  double max_x = blk_pins_[0].ComponentPtr()->X();
  double min_x = max_x;
  double tmp_pin_loc = 0;
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 1; i < sz; ++i) {
    tmp_pin_loc = blk_pins_[i].ComponentPtr()->X();
    if (max_x < tmp_pin_loc) {
      max_x = tmp_pin_loc;
      max_x_pin_id_ = i;
    }
    if (min_x > tmp_pin_loc) {
      min_x = tmp_pin_loc;
      min_x_pin_id_ = i;
    }
  }
}

void Net::UpdateMaxMinCtoCY() {
  if (blk_pins_.empty()) return;
  max_y_pin_id_ = 0;
  min_y_pin_id_ = 0;
  double max_y = blk_pins_[0].ComponentPtr()->Y();
  double min_y = max_y;
  double tmp_pin_loc = 0;
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 1; i < sz; ++i) {
    tmp_pin_loc = blk_pins_[i].ComponentPtr()->Y();
    if (max_y < tmp_pin_loc) {
      max_y = tmp_pin_loc;
      max_y_pin_id_ = i;
    }
    if (min_y > tmp_pin_loc) {
      min_y = tmp_pin_loc;
      min_y_pin_id_ = i;
    }
  }
}

void Net::UpdateMaxMinCtoC() {
  UpdateMaxMinIdX();
  UpdateMaxMinIdY();
}

int Net::MaxPinCtoCX() {
  int max_pin_index = 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double max_x = component->X();
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; i++) {
    component = blk_pins_[i].ComponentPtr();
    if (max_x < component->X()) {
      max_x = component->X();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCX() {
  int min_pin_index = 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double min_x = component->X();
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; i++) {
    component = blk_pins_[i].ComponentPtr();
    if (min_x > component->X()) {
      min_x = component->X();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

int Net::MaxPinCtoCY() {
  int max_pin_index = 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double max_y = component->Y();
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; i++) {
    component = blk_pins_[i].ComponentPtr();
    if (max_y < component->Y()) {
      max_y = component->Y();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

int Net::MinPinCtoCY() {
  int min_pin_index = 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double min_y = component->Y();
  int sz = static_cast<int>(blk_pins_.size());
  for (int i = 0; i < sz; i++) {
    component = blk_pins_[i].ComponentPtr();
    if (min_y > component->Y()) {
      min_y = component->Y();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

double Net::HPWLCtoCX() {
  if (blk_pins_.empty()) return 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double max_x = component->X();
  double min_x = component->X();

  for (auto& pin : blk_pins_) {
    component = pin.ComponentPtr();
    if (max_x < component->X()) {
      max_x = component->X();
    }
    if (min_x > component->X()) {
      min_x = component->X();
    }
  }

  return (max_x - min_x) * weight_;
}

double Net::HPWLCtoCY() {
  if (blk_pins_.empty()) return 0;
  auto* component = blk_pins_[0].ComponentPtr();
  double max_y = component->Y();
  double min_y = component->Y();

  for (auto& pin : blk_pins_) {
    component = pin.ComponentPtr();
    if (max_y < component->Y()) {
      max_y = component->Y();
    }
    if (min_y > component->Y()) {
      min_y = component->Y();
    }
  }

  return (max_y - min_y) * weight_;
}

double Net::HPWLCtoC() { return HPWLCtoCX() + HPWLCtoCY(); }

}  // namespace dali
