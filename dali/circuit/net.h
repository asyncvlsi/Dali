//
// Created by Yihang Yang on 5/23/19.
//

#ifndef DALI_DALI_CIRCUIT_NET_H_
#define DALI_DALI_CIRCUIT_NET_H_

#include <string>
#include <vector>

#include "block.h"
#include "blockpinpair.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"

namespace dali {

class NetAux;
class IoPin;

/****
 * This is a class for nets. When initializing a net, its capacity and weight need
 * to be specified.
 * One can use AddBlockPinPair() and AddIoPin() to add pins to a net.
 */
class Net {
  public:
    Net(
        std::pair<const std::string, int> *name_num_pair_ptr,
        int capacity,
        double weight
    );

    // get the name
    const std::string &Name() const;

    // get the internal index
    int Id() const;

    // add block/pin pair to this net
    void AddBlockPinPair(Block *block_ptr, Pin *pin_ptr);

    std::vector<BlkPinPair> &BlockPins();

    // add an I/O pin to this net
    void AddIoPin(IoPin *io_pin);

    std::vector<IoPin *> &IoPinPtrs();

    // set the net weight
    void SetWeight(double weight);

    // get the net weight
    double Weight() const;

    // get the number of pins in this net
    int PinCnt() const;

    // get 1/(p-1), where p in the number of pins in this net
    double InvP() const;

    // set auxiliary information
    void SetAux(NetAux *aux);

    // get auxiliary information
    NetAux *Aux();

    // if a given block is not in this net, what is the lower and upper bound of this net in the x direction
    void GetXBoundIfBlkAbsent(Block *blk_ptr, double &lo, double &hi);

    // if a given block is not in this net, what is the lower and upper bound of this net in the y direction
    void GetYBoundIfBlkAbsent(Block *blk_ptr, double &lo, double &hi);

    // sort block pins based on block ids and pin ids
    void SortBlkPinList();

    // find the indices for pins with maximum x location and minimum x location in this net
    void UpdateMaxMinIdX();

    // find the indices for pins with maximum y location and minimum y location in this net
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
    std::vector<BlkPinPair> blk_pins_;
    std::vector<IoPin *> iopin_ptrs_;

    // cached data
    int max_pin_x_, min_pin_x_;
    int max_pin_y_, min_pin_y_;
    // 1.0/(p-1), where p is the number of pins connected by this net
    double inv_p_;
    int driver_pin_index = -1;

    // auxiliary information
    NetAux *aux_ptr_;
};

class NetAux {
  protected:
    Net *net_ptr_;
  public:
    explicit NetAux(Net *net_ptr)
        : net_ptr_(net_ptr) { net_ptr_->SetAux(this); }
    Net *GetNet() const { return net_ptr_; }
};

}

#endif //DALI_DALI_CIRCUIT_NET_H_
