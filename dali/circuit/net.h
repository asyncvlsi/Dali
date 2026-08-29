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

#ifndef DALI_CIRCUIT_NET_H_
#define DALI_CIRCUIT_NET_H_

#include <string>
#include <vector>

#include "component.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "net_pin.h"

namespace dali {

class NetAux;
class IoPin;

/** Electrical net with connected component pins, I/O pins, and HPWL helpers. */
class Net {
 public:
  Net(std::pair<const std::string, int>* name_id_pair_ptr, size_t capacity,
      double weight);

  /** Return the net name. */
  const std::string& Name() const;

  /** Return the net id. */
  int Id() const;

  /** Add a connected component/pin pair. */
  void AddComponentPinPair(Component* component_ptr, Pin* pin_ptr);

  /** Return connected component pins. */
  std::vector<NetPin>& ComponentPins();

  /** Add a connected I/O pin. */
  void AddIoPin(IoPin* io_pin);

  /** Return connected I/O pins. */
  std::vector<IoPin*>& IoPinPtrs();

  /**
   * Disconnect every pin, leaving the net in place but electrically inert.
   *
   * Nets are addressed by index throughout the placer, so erasing one would
   * renumber the rest and invalidate every id held elsewhere. Retiring instead
   * keeps the slot and empties it, which is what a netlist edit needs: splicing
   * a cell into a chain interrupts an existing connection, and the interrupted
   * net has to stop connecting what it used to.
   *
   * A retired net leaves the placement problem on its own -- the quadratic
   * builder skips anything with one pin or fewer -- so no caller needs to learn
   * about a new state. Connected components are updated to drop it from their
   * net lists, so the disconnection is symmetric.
   */
  void Retire();

  /** Set net weight and refresh the cached quadratic-placement coefficient. */
  void SetWeight(double weight);

  /** Return net weight used by wirelength metrics. */
  double Weight() const;

  /** Return connected component-pin count used by placement models. */
  size_t PinCnt() const;

  /** Return cached weighted 1/(p-1), where p is PinCnt(). */
  double InvP() const;

  /** Attach auxiliary data owned by a downstream algorithm. */
  void SetAux(NetAux* aux);

  /** Return attached auxiliary data, if any. */
  NetAux* Aux();

  /** Return x bounds if component_ptr were excluded from this net. */
  void GetXBoundIfComponentAbsent(Component* component_ptr, double& lo,
                                  double& hi);

  /** Return y bounds if component_ptr were excluded from this net. */
  void GetYBoundIfComponentAbsent(Component* component_ptr, double& lo,
                                  double& hi);

  /** Sort component pins by component id, then pin id. */
  void SortComponentPinList();

  /** Update cached indices of min/max x component pins. */
  void UpdateMaxMinIdX();

  /** Update cached indices of min/max y component pins. */
  void UpdateMaxMinIdY();

  /** Update cached min/max pin indices in both dimensions. */
  void UpdateMaxMinIndex();

  /** Return index of the component pin with maximum absolute x location. */
  int MaxComponentPinIdX() const;

  /** Return index of the component pin with minimum absolute x location. */
  int MinComponentPinIdX() const;

  /** Return index of the component pin with maximum absolute y location. */
  int MaxComponentPinIdY() const;

  /** Return index of the component pin with minimum absolute y location. */
  int MinComponentPinIdY() const;

  /** Return component pointer for the maximum-x component pin. */
  Component* MaxComponentPtrX() const;

  /** Return component pointer for the minimum-x component pin. */
  Component* MinComponentPtrX() const;

  /** Return component pointer for the maximum-y component pin. */
  Component* MaxComponentPtrY() const;

  /** Return component pointer for the minimum-y component pin. */
  Component* MinComponentPtrY() const;

  /** Return weighted x-direction HPWL for component pins. */
  double WeightedHPWLX();

  /** Return unweighted x-direction HPWL for component pins. */
  double HPWLX();

  /** Return unweighted y-direction HPWL for component pins. */
  double HPWLY();

  /** Return weighted y-direction HPWL for component pins. */
  double WeightedHPWLY();

  /** Return weighted HPWL for component pins. */
  double WeightedHPWL();

  /** Return weighted x span of component pin bounding boxes. */
  double WeightedBboxX();

  /** Return weighted y span of component pin bounding boxes. */
  double WeightedBboxY();

  /** Return weighted bounding box span for component pin shapes. */
  double WeightedBbox();

  /** Return cached lower x bound after UpdateMaxMinIdX(). */
  double MinX() const;

  /** Return cached upper x bound after UpdateMaxMinIdX(). */
  double MaxX() const;

  /** Return cached lower y bound after UpdateMaxMinIdY(). */
  double MinY() const;

  /** Return cached upper y bound after UpdateMaxMinIdY(). */
  double MaxY() const;

  /** Update cached extreme component centers in x direction. */
  void UpdateMaxMinCtoCX();

  /** Update cached extreme component centers in y direction. */
  void UpdateMaxMinCtoCY();

  /** Update cached extreme component-pin locations in both directions. */
  void UpdateMaxMinCtoC();

  /** Return index of component pin with maximum component-center x. */
  int MaxPinCtoCX();

  /** Return index of component pin with minimum component-center x. */
  int MinPinCtoCX();

  /** Return index of component pin with maximum component-center y. */
  int MaxPinCtoCY();

  /** Return index of component pin with minimum component-center y. */
  int MinPinCtoCY();

  /** Return weighted x-direction HPWL using component centers. */
  double HPWLCtoCX();

  /** Return weighted y-direction HPWL using component centers. */
  double HPWLCtoCY();

  /** Return weighted HPWL using component centers. */
  double HPWLCtoC();

 protected:
  std::pair<const std::string, int>* name_id_pair_ptr_;
  double weight_;
  int cnt_fixed_;
  std::vector<NetPin> component_pins_;
  std::vector<IoPin*> iopin_ptrs_;

  // Cached extreme component-pin indices.
  int max_x_pin_id_, min_x_pin_id_;
  int max_y_pin_id_, min_y_pin_id_;
  // weight_/(p-1), where p is the number of connected component pins.
  double inv_p_;
  int driver_pin_index = -1;

  // auxiliary information
  NetAux* aux_ptr_;
};

class NetAux {
 public:
  explicit NetAux(Net* net_ptr) : net_ptr_(net_ptr) { net_ptr_->SetAux(this); }
  Net* GetNet() const { return net_ptr_; }

 protected:
  Net* net_ptr_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_NET_H_
