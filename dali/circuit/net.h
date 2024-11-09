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

/****
 * This is a class for nets. When initializing a net, its capacity and weight
 * need to be specified. One can use AddBlkPinPair() and AddIoPin() to add pins
 * to a net.
 */
class Net {
 public:
  Net(std::pair<const std::string, int> *name_id_pair_ptr, size_t capacity,
      double weight);

  // get the name of this net
  const std::string &Name() const;

  // get the internal index
  int Id() const;

  // add block/pin pair to this net
  void AddBlkPinPair(Block *block_ptr, Pin *pin_ptr);

  std::vector<NetPin> &BlockPins();

  // add an I/O pin to this net
  void AddIoPin(IoPin *io_pin);

  std::vector<IoPin *> &IoPinPtrs();

  // set the net weight
  void SetWeight(double weight);

  // get the net weight
  double Weight() const;

  // get the number of pins in this net
  size_t PinCnt() const;

  // get 1/(p-1), where p in the number of pins in this net
  double InvP() const;

  // set auxiliary information
  void SetAux(NetAux *aux);

  // get auxiliary information
  NetAux *Aux();

  // if a given block is not in this net, what is the lower and upper bound of
  // this net in the x direction
  void GetXBoundIfBlkAbsent(Block *blk_ptr, double &lo, double &hi);

  // if a given block is not in this net, what is the lower and upper bound of
  // this net in the y direction
  void GetYBoundIfBlkAbsent(Block *blk_ptr, double &lo, double &hi);

  // sort block pins based on block ids and pin ids
  void SortBlkPinList();

  // find the indices for pins with maximum x location and minimum x location in
  // this net
  void UpdateMaxMinIdX();

  // find the indices for pins with maximum y location and minimum y location in
  // this net
  void UpdateMaxMinIdY();

  // find the indices for pins in both directions
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
  Block *MaxBlkPtrX() const;

  // get the Block pointer of the BlockPin pair with the minimum x location
  Block *MinBlkPtrX() const;

  // get the Block pointer of the BlockPin pair with the maximum y location
  Block *MaxBlkPtrY() const;

  // get the Block pointer of the BlockPin pair with the minimum y location
  Block *MinBlkPtrY() const;

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
  std::pair<const std::string, int> *name_id_pair_ptr_;
  double weight_;
  int cnt_fixed_;
  std::vector<NetPin> blk_pins_;
  std::vector<IoPin *> iopin_ptrs_;

  // cached data
  int max_x_pin_id_, min_x_pin_id_;
  int max_y_pin_id_, min_y_pin_id_;
  // 1.0/(p-1), where p is the number of pins connected by this net
  double inv_p_;
  int driver_pin_index = -1;

  // auxiliary information
  NetAux *aux_ptr_;
};

class NetAux {
 public:
  explicit NetAux(Net *net_ptr) : net_ptr_(net_ptr) { net_ptr_->SetAux(this); }
  Net *GetNet() const { return net_ptr_; }

 protected:
  Net *net_ptr_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_NET_H_
