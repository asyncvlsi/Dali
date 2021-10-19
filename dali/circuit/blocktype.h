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

namespace dali {

/****
 * The class BlockType is an abstraction of a kind of gate, like INV, NAND, C-element
 * Here are the attributes of BlockType:
 *  name: the name of this kind of gate
 *  width: the width of its boundary
 *  height: the height of its boundary
 *  well: this is for gridded cell placement, N/P-well shapes are needed for alignment in a local cluster
 *  pin: a list of cell pins with shapes and offsets specified
 * ****/

class BlockTypeWell;

class BlockType {
  public:
    BlockType(const std::string *name_ptr, int width, int height);

    // set the name of this BlockType
    void SetName(const std::string *name_ptr);

    // get the pointer to the const name
    const std::string *NamePtr() const { return name_ptr_; }

    const std::string &Name() { return *name_ptr_; }

    // check if a pin with a given name exists in this BlockType or not
    bool IsPinExisting(std::string &pin_name) {
        return pin_name_num_map_.find(pin_name) != pin_name_num_map_.end();
    }

    // return the index of a pin with a given name
    int GetPinIndex(std::string &pin_name);

    // return a pointer to a newly allocated location for a Pin with a given name
    // if this member function is used to create pins, one needs to set pin shapes using the return pointer
    Pin *AddPin(std::string &pin_name, bool is_input);

    // add a pin with a given name and x/y offset
    void AddPin(std::string &pin_name, double x_offset, double y_offset);

    // get the pointer to the pin with the given name
    // if such a pin does not exist, the return value is nullptr
    Pin *GetPinPtr(std::string &pin_name);

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

    // check if the pin_list_ is empty
    bool ContainNoPin() const { return pin_list_.empty(); }

    // report the information of this BlockType for debugging purposes
    void Report() const;

  private:
    const std::string *name_ptr_;
    int width_, height_;
    long int area_;
    BlockTypeWell *well_ptr_ = nullptr;
    std::vector<Pin> pin_list_;
    std::map<std::string, int> pin_name_num_map_;
};

}

#endif //DALI_DALI_CIRCUIT_BLOCKTYPE_H_
