//
// Created by Yihang Yang on 10/31/19.
//

#include "blocktype.h"

namespace dali {

BlockType::BlockType(
    const std::string *name_ptr,
    int width,
    int height
) : name_ptr_(name_ptr),
    width_(width),
    height_(height),
    area_((long int) width_ * (long int) height_),
    well_ptr_(nullptr) {}

void BlockType::SetName(const std::string *name_ptr) {
    DaliExpects(name_ptr != nullptr,
                "Cannot set BlockType name_ptr to nullptr\n");
    name_ptr_ = name_ptr;
}

int BlockType::GetPinIndex(std::string &pin_name) {
    auto ret = pin_name_num_map_.find(pin_name);
    if (ret != pin_name_num_map_.end()) {
        return ret->second;
    }
    return -1;
}

Pin *BlockType::AddPin(std::string &pin_name, bool is_input) {
    bool pin_not_exist =
        (pin_name_num_map_.find(pin_name) == pin_name_num_map_.end());
    DaliExpects(pin_not_exist,
                "Cannot add this pin in BlockType: " + *NamePtr()
                    + ", because this pin exists in blk_pin_list: "
                    + pin_name);
    pin_name_num_map_.insert(std::pair<std::string, int>(pin_name,
                                                         pin_list_.size()));
    std::pair<const std::string, int>
        *name_num_ptr = &(*pin_name_num_map_.find(pin_name));
    pin_list_.emplace_back(name_num_ptr, this);
    pin_list_.back().SetIOType(is_input);
    return &pin_list_.back();
}

void BlockType::AddPin(std::string &pin_name,
                       double x_offset,
                       double y_offset) {
    bool pin_not_exist =
        pin_name_num_map_.find(pin_name) == pin_name_num_map_.end();
    DaliExpects(pin_not_exist,
                "Cannot add this pin, because this pin exists in blk_pin_list: "
                    + pin_name);
    pin_name_num_map_.insert(std::pair<std::string, int>(pin_name,
                                                         pin_list_.size()));
    std::pair<const std::string, int>
        *name_num_ptr = &(*pin_name_num_map_.find(pin_name));
    pin_list_.emplace_back(name_num_ptr, this, x_offset, y_offset);
}

Pin *BlockType::GetPinPtr(std::string &pin_name) {
    auto res = pin_name_num_map_.find(pin_name);
    return res == pin_name_num_map_.end() ? nullptr
                                          : &(pin_list_[res->second]);
}

void BlockType::SetWell(BlockTypeWell *well_ptr) {
    DaliExpects(well_ptr != nullptr,
                "Cannot set @param well_ptr to nullptr in function: BlockType::SetWell()");
    well_ptr_ = well_ptr;
}

void BlockType::SetWidth(int width) {
    width_ = width;
    area_ = (long int) width_ * (long int) height_;
}

void BlockType::SetHeight(int height) {
    height_ = height;
    area_ = (long int) width_ * (long int) height_;
}

void BlockType::SetSize(int width, int height) {
    width_ = width;
    height_ = height;
    area_ = (long int) width_ * (long int) height_;
}

void BlockType::Report() const {
    BOOST_LOG_TRIVIAL(info)
        << "  BlockType name: " << *NamePtr() << "\n"
        << "    width, height: " << Width() << " "
        << Height() << "\n"
        << "    pin list:\n";
    for (auto &it: pin_name_num_map_) {
        BOOST_LOG_TRIVIAL(info)
            << "      " << it.first << " " << it.second << " ("
            << pin_list_[it.second].OffsetX() << ", "
            << pin_list_[it.second].OffsetY() << ")\n";
        pin_list_[it.second].Report();
    }
}

}
