//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_DALI_CIRCUIT_BLOCKTYPE_H_
#define DALI_DALI_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "pin.h"

/****
 * This header file contains class BlockType and BlockTypeWell. The former contains
 * some basic physical information of a kind of gate, and the later contains
 * N/P-well physical information.
 * ****/

namespace dali {

class BlockTypeWell;

/****
 * The class BlockType is an abstraction of a kind of gate, like INV, NAND, NOR
 * Here are the attributes of BlockType:
 *     name: the name of this kind of gate
 *     width: the width of its placement and routing boundary
 *     height: the height of its placement and routing boundary
 *     well: this is for gridded cell placement, N/P-well shapes are needed for alignment in a local cluster
 *     pin: a list of cell pins with shapes and offsets
 */
class BlockType {
  public:
    BlockType(const std::string *name_ptr, int width, int height);

    const std::string &Name() const { return *name_ptr_; }

    // check if a pin with a given name exists in this BlockType or not
    bool IsPinExisting(std::string const &pin_name) const {
        return pin_name_id_map_.find(pin_name) != pin_name_id_map_.end();
    }

    // return the index of a pin with a given name
    int GetPinId(std::string const &pin_name) const;

    // return a pointer to a newly allocated location for a Pin with a given name
    // if this member function is used to create pins, one needs to set pin shapes using the return pointer
    Pin *AddPin(std::string const &pin_name, bool is_input);

    // add a pin with a given name and x/y offset
    void AddPin(std::string const &pin_name, double x_offset, double y_offset);

    // get the pointer to the pin with the given name
    // if such a pin does not exist, the return value is nullptr
    Pin *GetPinPtr(std::string const &pin_name);

    // set the N/P-well information for this BlockType
    void SetWell(BlockTypeWell *well_ptr);

    // get the pointer to the well of this BlockType
    BlockTypeWell *WellPtr() const { return well_ptr_; }

    // set the width of this BlockType and update its area
    void SetWidth(int width);

    // get the width of this BlockType
    int Width() const { return width_; }

    // set the height of this BlockType and update its area
    void SetHeight(int height);

    void SetSize(int width, int height);

    // get the height of this BlockType
    int Height() const { return height_; }

    // get the area of this BlockType
    long int Area() const { return area_; }

    // get the pointer to the list of cell pins
    std::vector<Pin> &PinList() { return pin_list_; }

    // report the information of this BlockType for debugging purposes
    void Report() const;

  private:
    const std::string *name_ptr_;
    int width_, height_;
    long int area_;
    BlockTypeWell *well_ptr_ = nullptr;
    std::vector<Pin> pin_list_;
    std::map<std::string, int> pin_name_id_map_;
};


/****
 * This struct BlockTypeWell provides the N/P-well geometries for a BlockType.
 * Assumptions:
 *  1. BlockType has at most one rectangular N-well and at most one rectangular P-well.
 *  2. It is allowed to provide only N-well or P-well.
 *  3. If both N-well and P-well are present, they must be abutted.
 *     This is for debugging purposes, also for compact physical layout.
 *     +-----------------+
 *     |                 |
 *     |                 |
 *     |   N-well        |
 *     |                 |
 *     |                 |
 *     +-----------------+  p_n_edge_
 *     |                 |
 *     |                 |
 *     |    P-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+
 * ****/
class BlockTypeWell {
  public:
    explicit BlockTypeWell(BlockType *type_ptr) : type_ptr_(type_ptr) {}

    // get the pointer to the BlockType this well belongs to
    BlockType *BlkTypePtr() const { return type_ptr_; }

    // set the rect of N-well
    void SetNwellRect(int lx, int ly, int ux, int uy);

    // get the rect of N-well
    const RectI &NwellRect() { return n_rect_; }

    // set the rect of P-well
    void SetPwellRect(int lx, int ly, int ux, int uy);

    // get the rect of P-well
    const RectI &PwellRect() { return p_rect_; }

    // get the P/N well boundary
    int PnBoundary() const { return p_n_edge_; }

    // get the height of N-well
    int Nheight() const { return type_ptr_->Height() - p_n_edge_; }

    // get the height of P-well
    int Pheight() const { return p_n_edge_; }

    // set the rect of N or P well
    void SetWellRect(bool is_n, int lx, int ly, int ux, int uy);

    // check if N-well is abutted with P-well, if both exist
    bool IsNpWellAbutted() const;

    // report the information of N/P-well for debugging purposes
    void Report() const;

  private:
    BlockType *type_ptr_; // pointer to BlockType
    bool is_n_set_ = false; // whether N-well shape is set or not
    bool is_p_set_ = false; // whether P-well shape is set or not
    RectI n_rect_; // N-well rect
    RectI p_rect_; // P-well rect
    int p_n_edge_ = 0; // cached N/P-well boundary
};

}

#endif //DALI_DALI_CIRCUIT_BLOCKTYPE_H_
