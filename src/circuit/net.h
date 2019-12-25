//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef DALI_NET_HPP
#define DALI_NET_HPP

#include <string>
#include <vector>
#include "block.h"
#include "blockpinpair.h"
#include "netaux.h"
#include "../common/misc.h"

class NetAux;

class Net {
 protected:
  std::pair<const std::string, int> *name_num_pair_ptr_;
  double weight_;
  int cnt_fixed_;

  // cached data
  int max_pin_x_, min_pin_x_;
  int max_pin_y_, min_pin_y_;

  NetAux *aux_;
 public:
  explicit Net(std::pair<const std::string, int> *name_num_pair_ptr, double weight = 1);

  std::vector<BlockPinPair> blk_pin_list;
  void AddBlockPinPair(Block *block_ptr, int pin_index);

  const std::string *Name() const;
  int Num();
  void SetWeight(double weight);
  double Weight() const;
  double InvP();
  int P();
  int FixedCnt();
  void SetAux(NetAux *aux);
  NetAux *Aux();

  void XBoundExclude(Block *blk_ptr, double &x1, double &x2);
  void YBoundExclude(Block *blk_ptr, double &y1, double &y2);

  void SortBlkPinList();
  void UpdateMaxMinX();
  void UpdateMaxMinY();
  void UpdateMaxMin();
  int MaxBlkPinNumX();
  int MinBlkPinNumX();
  int MaxBlkPinNumY();
  int MinBlkPinNumY();
  Block *MaxBlockX();
  Block *MinBlockX();
  Block *MaxBlockY();
  Block *MinBlockY();
  double HPWLX();
  double HPWLY();
  double HPWL();

  double Left()   {return blk_pin_list[min_pin_x_].AbsX();}
  double Right()  {return blk_pin_list[max_pin_x_].AbsX();}
  double Bottom() {return blk_pin_list[min_pin_y_].AbsY();}
  double Top()    {return blk_pin_list[max_pin_y_].AbsY();}

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

  /*friend std::ostream& operator<<(std::ostream& os, const Net &net) {
    os << *net.Name() << "  " << net.Weight() << "\n";
    for (auto &&block_pin_pair: net.blk_pin_list) {
      os << "\t" << block_pin_pair << "\n";
    }
    return os;
  }*/
};

#endif //DALI_NET_HPP
