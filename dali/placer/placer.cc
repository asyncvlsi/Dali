//
// Created by Yihang Yang on 5/23/19.
//

#include "placer.h"

#include <cmath>

#include <algorithm>

#include "dali/common/misc.h"

namespace dali {

Placer::Placer() {
    aspect_ratio_ = 0;
    filling_rate_ = 0;
    left_ = 0;
    right_ = 0;
    bottom_ = 0;
    top_ = 0;
    circuit_ = nullptr;
}

Placer::Placer(double aspect_ratio, double filling_rate) : aspect_ratio_(
    aspect_ratio), filling_rate_(filling_rate) {
    left_ = 0;
    right_ = 0;
    bottom_ = 0;
    top_ = 0;
    circuit_ = nullptr;
}

double Placer::GetBlkHPWL(Block &blk) {
    double hpwl = 0;
    std::vector<Net> &net_list = *(NetList());
    for (auto &idx: *blk.NetList()) {
        hpwl += net_list[idx].WeightedHPWL();
    }
    return hpwl;
}

bool Placer::IsBoundaryProper() {
    if (circuit_->MaxBlkWidth() > RegionRight() - RegionLeft()) {
        BOOST_LOG_TRIVIAL(info) << "Improper boundary:\n"
                                << "    maximum cell width is larger than the width of placement region\n";
        return false;
    }
    if (circuit_->MaxBlkHeight() > RegionTop() - RegionBottom()) {
        BOOST_LOG_TRIVIAL(info) << "Improper boundary:\n"
                                << "    maximum cell height is larger than the height of placement region\n";
        return false;
    }
    return true;
}

void Placer::SetBoundaryAuto() {
    DaliExpects(circuit_ != nullptr,
                "Must set input circuit before setting boundaries");
    long int tot_block_area = circuit_->TotBlkArea();
    int width = std::ceil(std::sqrt(
        double(tot_block_area) / aspect_ratio_ / filling_rate_));
    int height = std::ceil(width * aspect_ratio_);
    BOOST_LOG_TRIVIAL(info) << "Pre-set aspect ratio: " << aspect_ratio_
                            << "\n";
    aspect_ratio_ = height / (double) width;
    BOOST_LOG_TRIVIAL(info) << "Adjusted aspect rate: " << aspect_ratio_
                            << "\n";
    left_ = (int) (circuit_->AveBlkWidth());
    right_ = left_ + width;
    bottom_ = (int) (circuit_->AveBlkWidth());
    top_ = bottom_ + height;
    int area = height * width;
    BOOST_LOG_TRIVIAL(info) << "Pre-set filling rate: " << filling_rate_
                            << "\n";
    filling_rate_ = double(tot_block_area) / area;
    BOOST_LOG_TRIVIAL(info) << "Adjusted filling rate: " << filling_rate_
                            << "\n";
    DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundary(int left, int right, int bottom, int top) {
    DaliExpects(circuit_ != nullptr,
                "Must set input circuit before setting boundaries");
    DaliExpects(left < right,
                "Invalid boundary setting: left boundary should be less than right boundary!");
    DaliExpects(bottom < top,
                "Invalid boundary setting: bottom boundary should be less than top boundary!");
    unsigned long int tot_block_area = circuit_->TotBlkArea();
    unsigned long int tot_area = (right - left) * (top - bottom);
    DaliExpects(tot_area >= tot_block_area,
                "Invalid boundary setting: given region has smaller area than total block area!");
    BOOST_LOG_TRIVIAL(info) << "Pre-set filling rate: " << filling_rate_
                            << "\n";
    filling_rate_ = (double) tot_block_area / (double) tot_area;
    BOOST_LOG_TRIVIAL(info) << "Adjusted filling rate: " << filling_rate_
                            << "\n";
    left_ = left;
    right_ = right;
    bottom_ = bottom;
    top_ = top;
    DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::SetBoundaryDef() {
    left_ = GetCircuit()->RegionLLX();
    right_ = GetCircuit()->RegionURX();
    bottom_ = GetCircuit()->RegionLLY();
    top_ = GetCircuit()->RegionURY();
    DaliExpects(IsBoundaryProper(), "Invalid boundary setting");
}

void Placer::ReportBoundaries() {
    BOOST_LOG_TRIVIAL(info) << "Left, Right, Bottom, Top:\n";
    BOOST_LOG_TRIVIAL(info) << "  "
                            << RegionLeft() << ", "
                            << RegionRight() << ", "
                            << RegionBottom() << ", "
                            << RegionTop() << "\n";
}

void Placer::UpdateAspectRatio() {
    if ((right_ - left_ == 0) || (top_ - bottom_ == 0)) {
        BOOST_LOG_TRIVIAL(fatal) << "Error!\n"
                                 << "Zero Height or Width of placement region!\n";
        ReportBoundaries();
        exit(1);
    }
    aspect_ratio_ = (top_ - bottom_) / (double) (right_ - left_);
}

bool Placer::StartPlacement() {
    BOOST_LOG_TRIVIAL(fatal)
        << "Error!\n"
        << "This function should not be called! You need to implement it yourself!\n";
    return false;
}

void Placer::TakeOver(Placer *placer) {
    aspect_ratio_ = placer->AspectRatio();
    filling_rate_ = placer->FillingRate();
    left_ = placer->RegionLeft();
    right_ = placer->RegionRight();
    bottom_ = placer->RegionBottom();
    top_ = placer->RegionTop();
    circuit_ = placer->GetCircuit();
}

void Placer::GenMATLABScriptPlaced(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    ost << RegionLeft() << " " << RegionBottom() << " "
        << RegionRight() - RegionLeft() << " "
        << RegionTop() - RegionBottom() << "\n";
    for (auto &block: *BlockList()) {
        if (block.IsPlaced()) {
            ost << block.LLX() << " " << block.LLY() << " " << block.Width()
                << " " << block.Height() << "\n";
        }
    }
    ost.close();
}

bool Placer::SaveNodeTerminal(std::string const &terminal_file,
                              std::string const &node_file) {
    std::ofstream ost(terminal_file.c_str());
    std::ofstream ost1(node_file.c_str());
    DaliExpects(ost.is_open() && ost1.is_open(),
                "Cannot open file " + terminal_file + " or " + node_file);
    for (auto &block: *BlockList()) {
        if (block.IsMovable()) {
            ost1 << block.X() << "\t" << block.Y() << "\n";
        } else {
            double low_x, low_y, width, height;
            width = block.Width();
            height = block.Height();
            low_x = block.LLX();
            low_y = block.LLY();
            for (int j = 0; j < height; j++) {
                ost << low_x << "\t" << low_y + j << "\n";
                ost << low_x + width << "\t" << low_y + j << "\n";
            }
            for (int j = 0; j < width; j++) {
                ost << low_x + j << "\t" << low_y << "\n";
                ost << low_x + j << "\t" << low_y + height << "\n";
            }
        }
    }
    ost.close();
    ost1.close();
    return true;
}

void Placer::SaveDEFFile(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    // 1. print file header?
    ost << "VERSION 5.8 ;\n"
        << "DIVIDERCHAR \"/\" ;\n"
        << "BUSBITCHARS \"[]\" ;\n";
    ost << "DESIGN tmp_circuit_name\n";
    ost
        << "UNITS DISTANCE MICRONS 2000 \n\n"; // temporary values, depends on LEF file

    // no core rows?
    // no tracks?

    // 2. print component
    BOOST_LOG_TRIVIAL(info) << BlockList()->size() << "\n";
    ost << "COMPONENTS " << BlockList()->size() << " ;\n";
    for (auto &block: *BlockList()) {
        ost << "- "
            << *(block.NamePtr()) << " "
            << *(block.TypePtr()->NamePtr()) << " + "
            << "PLACED"
            << " ("
            << " "
                + std::to_string((int) (block.LLX()
                    * circuit_->design_.def_distance_microns
                    * circuit_->GridValueX()))
            << " "
                + std::to_string((int) (block.LLY()
                    * circuit_->design_.def_distance_microns
                    * circuit_->GridValueY()))
            << " ) "
            << OrientStr(block.Orient()) + " ;\n";
    }
    ost << "END COMPONENTS\n";

    // 3. print net
    ost << "NETS " << NetList()->size() << " ;\n";
    for (auto &net: *NetList()) {
        ost << "- "
            << *(net.Name()) << "\n";
        ost << " ";
        for (auto &pin_pair: net.blk_pin_list) {
            ost << " ( " << *(pin_pair.BlockNamePtr()) << " "
                << *(pin_pair.PinNamePtr()) << " ) ";
        }
        ost << "\n" << " ;\n";
    }
    ost << "END NETS\n\n";
    ost << "END DESIGN\n";
}

void Placer::SaveDEFFile(std::string const &name_of_file,
                         std::string const &input_def_file) {
    circuit_->SaveDefFile(name_of_file, input_def_file);
}

void Placer::EmitDEFWellFile(std::string const &name_of_file,
                             int well_emit_mode,
                             bool enable_emitting_cluster) {
    BOOST_LOG_TRIVIAL(info)
        << "virtual function Placer::EmitDEFWellFile() does nothing, you should not use this member function\n";
}

void Placer::SanityCheck() {
    double epsilon = 1e-3;
    DaliExpects(filling_rate_ > epsilon,
                "Filling rate should be in a proper range, for example [0.1, 1], current value: "
                    + std::to_string(filling_rate_));
    for (auto &net: *NetList()) {
        DaliWarns(net.blk_pin_list.empty(),
                  "Empty net or this net only contains unplaced IOPINs: "
                      + *net.Name());
    }
    DaliExpects(IsBoundaryProper(), "Improper boundary setting");
    for (auto &pair: circuit_->tech_.block_type_map_) {
        BlockType *blk_type_ptr = pair.second;
        for (auto &pin: *(blk_type_ptr->PinList())) {
            DaliExpects(!pin.RectEmpty(),
                        "No RECT found for pin: " + *(blk_type_ptr->NamePtr())
                            + "::" + *(pin.Name()));
        }
    }
}

void Placer::UpdateMovableBlkPlacementStatus() {
    for (auto &blk: *BlockList()) {
        if (blk.IsMovable()) {
            blk.SetPlacementStatus(PLACED);
        }
    }
}

void Placer::ShiftX(double shift_x) {
    for (auto &block: *BlockList()) {
        block.IncreaseX(shift_x);
    }
}

void Placer::ShiftY(double shift_y) {
    for (auto &block: *BlockList()) {
        block.IncreaseY(shift_y);
    }
}

}
