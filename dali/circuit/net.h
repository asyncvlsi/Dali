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

#include "block.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "net_pin.h"

namespace dali {

class NetAux;
class IoPin;

/** Electrical net with connected block pins, I/O pins, and HPWL helpers. */
class Net {
 public:
  Net(std::pair<const std::string, int>* name_id_pair_ptr, size_t capacity,
      double weight);

  /** Return the net name. */
  const std::string& Name() const;

  /** Return the net id. */
  int Id() const;

  /** Add a connected block/pin pair. */
  void AddBlkPinPair(Block* block_ptr, Pin* pin_ptr);

  /** Return connected block pins. */
  std::vector<NetPin>& BlockPins();

  /** Add a connected I/O pin. */
  void AddIoPin(IoPin* io_pin);

  /** Return connected I/O pins. */
  std::vector<IoPin*>& IoPinPtrs();

  /** Set net weight used by wirelength metrics. */
  void SetWeight(double weight);

  /** Return net weight used by wirelength metrics. */
  double Weight() const;

  /** Return total connected block and I/O pin count. */
  size_t PinCnt() const;

  /** Return 1/(p-1), where p is PinCnt(). */
  double InvP() const;

  /** Attach auxiliary data owned by a downstream algorithm. */
  void SetAux(NetAux* aux);

  /** Return attached auxiliary data, if any. */
  NetAux* Aux();

  /** Return x bounds if blk_ptr were excluded from this net. */
  void GetXBoundIfBlkAbsent(Block* blk_ptr, double& lo, double& hi);

  /** Return y bounds if blk_ptr were excluded from this net. */
  void GetYBoundIfBlkAbsent(Block* blk_ptr, double& lo, double& hi);

  /** Sort block pins by block id, then pin id. */
  void SortBlkPinList();

  /** Update cached indices of min/max x block pins. */
  void UpdateMaxMinIdX();

  /** Update cached indices of min/max y block pins. */
  void UpdateMaxMinIdY();

  /** Update cached min/max pin indices in both dimensions. */
  void UpdateMaxMinIndex();

  // get the index of the BlockPin pair with the maximum x location
  int MaxBlkPinIdX() const;

  // get the index of the BlockPin pair with the minimum x location
  int MinBlkPinIdX() const;

  // get the index of the BlockPin pair with the maximum y location
  int MaxBlkPinIdY() const;

  // get the index of the BlockPin pair with the minimum y location
  int MinBlkPinIdY() const;

  // get the Block pointer of the BlockPin pair with the maximum x location
  Block* MaxBlkPtrX() const;

  // get the Block pointer of the BlockPin pair with the minimum x location
  Block* MinBlkPtrX() const;

  // get the Block pointer of the BlockPin pair with the maximum y location
  Block* MaxBlkPtrY() const;

  // get the Block pointer of the BlockPin pair with the minimum y location
  Block* MinBlkPtrY() const;

  // get the weighted HPWLX of this net
  double WeightedHPWLX();

  // get the weight HPWLY of this net
  double WeightedHPWLY();

  // get the weight HPWL of this net
  double WeightedHPWL();

  // get the weighted bounding box of this net
  double WeightedBboxX();

  // get the weight bounding box of this net
  double WeightedBboxY();

  // get the weight bounding box of this net
  double WeightedBbox();

  // get the lower x bound of this net
  double MinX() const;

  // get the upper x bound of this net
  double MaxX() const;

  // get the lower y bound of this net
  double MinY() const;

  // get the upper y bound of this net
  double MaxY() const;

  void UpdateMaxMinCtoCX();

  void UpdateMaxMinCtoCY();

  void UpdateMaxMinCtoC();

  int MaxPinCtoCX();

  int MinPinCtoCX();

  int MaxPinCtoCY();

  int MinPinCtoCY();

  double HPWLCtoCX();

  double HPWLCtoCY();

  double HPWLCtoC();

 protected:
  std::pair<const std::string, int>* name_id_pair_ptr_;
  double weight_;
  int cnt_fixed_;
  std::vector<NetPin> blk_pins_;
  std::vector<IoPin*> iopin_ptrs_;

  // cached data
  int max_x_pin_id_, min_x_pin_id_;
  int max_y_pin_id_, min_y_pin_id_;
  // 1.0/(p-1), where p is the number of pins connected by this net
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
