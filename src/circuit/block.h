//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_BLOCK_HPP
#define HPCC_BLOCK_HPP


#include <vector>
#include <map>
#include <string>
#include <iostream>
#include "status.h"
#include "blockaux.h"
#include "blocktype.h"
#include "common/misc.h"

/* a block can be a gate, can also be a large module, it includes information like
 * the Name of a gate/module, its Width and Height, its lower Left corner (LLX, LLY),
 * the movability, Orient. */

class BlockAux;

class Block {
 protected:
  /* essential data entries */
  BlockType *type_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  double llx_, lly_; // lower Left corner
  PlaceStatus place_status_;
  BlockOrient orient_; // currently not used
  BlockAux *aux_;
 public:
  bool is_placed = false;
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, bool movable = "true", BlockOrient orient = N);
  Block(BlockType *type, std::pair<const std::string, int>* name_num_pair, int llx, int lly, PlaceStatus place_state = UNPLACED, BlockOrient orient = N);
  std::vector<int> net_list;

  const std::string *Name() const;
  BlockType *Type() const;
  int Num() const;
  int Width() const;
  int Height() const;
  double LLX() const;
  double LLY() const;
  double URX() const;
  double URY() const;
  double X() const;
  double Y() const;
  PlaceStatus GetPlaceStatus() {return place_status_;}
  bool IsMovable() const;
  bool IsFixed() const;
  int Area() const;
  BlockOrient Orient() const {return orient_;}
  BlockAux *Aux();

  void SetLLX(double lower_left_x);
  void SetLLY(double lower_left_y);
  void SetURX(double upper_right_x);
  void SetURY(double upper_right_y);
  void SetCenterX(double center_x);
  void SetCenterY(double center_y);
  void SetPlaceStatus(PlaceStatus place_status) {place_status_ = place_status;}
  void SetOrient(BlockOrient &orient);
  void SetAux(BlockAux *aux);
  void SwapLoc(Block &blk);

  void IncreX(double displacement);
  void IncreY(double displacement);
  void IncreX(double displacement, double upper, double lower);
  void IncreY(double displacement, double upper, double lower);
  bool IsOverlap(const Block &rhs) const;
  bool IsOverlap(const Block *rhs) const;
  double OverlapArea(const Block &rhs) const;
  double OverlapArea(const Block *rhs) const;

  void Report();
  void ReportNet();

  /*friend std::ostream& operator<<(std::ostream& os, const Block &block) {
    os << "Block Name: " << *block.Name() << "\n";
    os << "Block Type: " << *(block.Type()->Name()) << "\n";
    os << "Width and Height: " << block.Width() << " " << block.Height() << "\n";
    os << "lower Left corner: " << block.LLX() << " " << block.LLY() << "\n";
    os << "movability: " << block.IsMovable() << "\n";
    os << "orientation: " << block.OrientStr() << "\n";
    os << "assigned primary key: " << block.Num() << "\n";
    return os;
  }*/

  const std::string *TypeName() const;
  std::string IsPlace() {return PlaceStatusStr(place_status_);}
  std::string LowerLeftCorner();
};

#endif //HPCC_BLOCK_HPP
