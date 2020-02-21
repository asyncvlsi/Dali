//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_NET_HPP
#define DALI_NET_HPP

#include <string>
#include <vector>

#include "block.h"
#include "blockpinpair.h"
#include "common/misc.h"

class NetAux;

class Net {
 protected:
  std::pair<const std::string, int> *name_num_pair_ptr_;
  double weight_;
  int cnt_fixed_;

  // cached data
  int max_pin_x_, min_pin_x_;
  int max_pin_y_, min_pin_y_;
  double inv_p;

  // auxiliary information
  NetAux *aux_;
 public:
  std::vector<BlockPinPair> blk_pin_list;
  explicit Net(std::pair<const std::string, int> *name_num_pair_ptr, double weight = 1);

  // API to add block/pin pair
  void AddBlockPinPair(Block *block_ptr, Pin *pin);

  const std::string *Name() const;
  std::string NameStr() const;
  int Num() const;
  void SetWeight(double weight);
  double Weight() const;
  double InvP() const;
  int P() const;
  int FixedCnt() const;
  void SetAux(NetAux *aux);
  NetAux *Aux() const;

  void XBoundExclude(Block *blk_ptr, double &lo, double &hi);
  void XBoundExclude(Block &blk_ptr, double &lo, double &hi);
  void YBoundExclude(Block *blk_ptr, double &lo, double &hi);
  void YBoundExclude(Block &blk_ptr, double &lo, double &hi);

  void SortBlkPinList();
  void UpdateMaxMinIndexX();
  void UpdateMaxMinIndexY();
  void UpdateMaxMinIndex();

  int MaxBlkPinNumX() const;
  int MinBlkPinNumX() const;
  int MaxBlkPinNumY() const;
  int MinBlkPinNumY() const;
  Block *MaxBlockX() const;
  Block *MinBlockX() const;
  Block *MaxBlockY() const;
  Block *MinBlockY() const;
  double HPWLX();
  double HPWLY();
  double HPWL();

  double MinX() const;
  double MaxX() const;
  double MinY() const;
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
};

class NetAux {
 protected:
  Net *net_;
 public:
  explicit NetAux(Net *net);
  Net *GetNet() const;
};

inline const std::string *Net::Name() const {
  return &(name_num_pair_ptr_->first);
}

inline std::string Net::NameStr() const {
  return name_num_pair_ptr_->first;
}

inline int Net::Num() const {
  return name_num_pair_ptr_->second;
}

inline void Net::SetWeight(double weight) {
  weight_ = weight;
}

inline double Net::Weight() const {
  return weight_;
}

inline double Net::InvP() const {
  return inv_p;
}

inline int Net::P() const {
  return (int) blk_pin_list.size();
}

inline int Net::FixedCnt() const {
  return cnt_fixed_;
}

inline void Net::SetAux(NetAux *aux) {
  assert(aux != nullptr);
  aux_ = aux;
}

inline NetAux *Net::Aux() const {
  return aux_;
}

inline void Net::XBoundExclude(Block &blk_ptr, double &lo, double &hi) {
  XBoundExclude(&blk_ptr, lo, hi);
}

inline void Net::YBoundExclude(Block &blk_ptr, double &lo, double &hi) {
  YBoundExclude(&blk_ptr, lo, hi);
}

inline int Net::MaxBlkPinNumX() const {
  return max_pin_x_;
}

inline void Net::UpdateMaxMinIndex() {
  UpdateMaxMinIndexX();
  UpdateMaxMinIndexY();
}

inline int Net::MinBlkPinNumX() const {
  return min_pin_x_;
}

inline int Net::MaxBlkPinNumY() const {
  return max_pin_y_;
}

inline int Net::MinBlkPinNumY() const {
  return min_pin_y_;
}

inline Block *Net::MaxBlockX() const {
  return blk_pin_list[max_pin_x_].GetBlock();
}

inline Block *Net::MinBlockX() const {
  return blk_pin_list[min_pin_x_].GetBlock();
}

inline Block *Net::MaxBlockY() const {
  return blk_pin_list[max_pin_y_].GetBlock();
}

inline Block *Net::MinBlockY() const {
  return blk_pin_list[min_pin_y_].GetBlock();
}

inline double Net::HPWL() {
  return HPWLX() + HPWLY();
}

inline double Net::MinX() const {
  return blk_pin_list[min_pin_x_].AbsX();
}

inline double Net::MaxX() const {
  return blk_pin_list[max_pin_x_].AbsX();
}

inline double Net::MinY() const {
  return blk_pin_list[min_pin_y_].AbsY();
}

inline double Net::MaxY() const {
  return blk_pin_list[max_pin_y_].AbsY();
}

inline Net *NetAux::GetNet() const {
  return net_;
}

#endif //DALI_NET_HPP
