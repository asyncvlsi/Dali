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
#ifndef DALI_CIRCUIT_NET_PIN_H_
#define DALI_CIRCUIT_NET_PIN_H_

#include "component.h"
#include "dali/common/misc.h"

namespace dali {

/**
 * Connection between a component instance and one of its macro pins.
 *
 * The class stores raw pointers for speed. Circuit construction must reserve
 * component and pin storage before creating net pins so vector growth does not
 * invalidate these pointers.
 */
class NetPin {
 public:
  NetPin(Component* component_ptr, Pin* pin_ptr)
      : component_ptr_(component_ptr), pin_ptr_(pin_ptr) {}

  /** Return the connected component. */
  Component* ComponentPtr() const { return component_ptr_; }

  /** Return the connected component id. */
  int ComponentId() const { return component_ptr_->Id(); }

  /** Return the connected pin. */
  Pin* PinPtr() const { return pin_ptr_; }

  /** Return the connected pin id. */
  int PinId() const { return pin_ptr_->Id(); }

  /** Return orientation-aware x offset from the component origin. */
  double OffsetX() const { return pin_ptr_->OffsetX(component_ptr_->Orient()); }

  /** Return orientation-aware y offset from the component origin. */
  double OffsetY() const { return pin_ptr_->OffsetY(component_ptr_->Orient()); }

  /** Return absolute x location of this pin. */
  double AbsX() const { return OffsetX() + component_ptr_->LLX(); }

  /** Return absolute y location of this pin. */
  double AbsY() const { return OffsetY() + component_ptr_->LLY(); }

  /** Return absolute pin location. */
  double2d Location() const { return double2d(AbsX(), AbsY()); }

  /** Return the connected component name. */
  const std::string& ComponentName() const { return component_ptr_->Name(); }

  /** Return the connected pin name. */
  const std::string& PinName() const { return pin_ptr_->Name(); }

  bool operator<(const NetPin& rhs) const {
    return (ComponentId() < rhs.ComponentId()) ||
           ((ComponentId() == rhs.ComponentId()) && (PinId() < rhs.PinId()));
  }
  bool operator>(const NetPin& rhs) const {
    return (ComponentId() > rhs.ComponentId()) ||
           ((ComponentId() == rhs.ComponentId()) && (PinId() > rhs.PinId()));
  }
  bool operator==(const NetPin& rhs) const {
    return (ComponentId() == rhs.ComponentId()) && (PinId() == rhs.PinId());
  }

 private:
  Component* component_ptr_;
  Pin* pin_ptr_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_NET_PIN_H_
