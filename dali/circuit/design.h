//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_DALI_CIRCUIT_DESIGN_H_
#define DALI_DALI_CIRCUIT_DESIGN_H_

#include <climits>

#include <unordered_map>
#include <vector>

#include "block.h"
#include "iopin.h"
#include "net.h"

namespace dali {

/****
 * This is a struct for getting the statistics of half-perimeter wire-length.
 */
struct NetHistogram {
    std::vector<int> bin_list_{2, 3, 4, 20, 40, 80, 160};
    std::vector<int> count_;
    std::vector<double> percent_;
    std::vector<double> sum_hpwl_;
    std::vector<double> ave_hpwl_;
    std::vector<double> min_hpwl_;
    std::vector<double> max_hpwl_;
    int tot_net_count_;
    double tot_hpwl_;
    double hpwl_unit_;
};

/****
 * This class contain basic information of a design (or information in a DEF file)
 */
class Design {
    friend class Circuit;
  public:
    // get the name of this design
    const std::string &Name() const { return name_; }

    // get DEF distance microns
    int DistanceMicrons() const { return distance_microns_; }

    // get the location of placement boundaries
    int RegionLeft() const { return region_left_; }
    int RegionRight() const { return region_right_; }
    int RegionBottom() const { return region_bottom_; }
    int RegionTop() const { return region_top_; }

    // locations of cells in Dali is on grid, but they do not have to be on grid
    // in a DEF file. The following two APIs return the offset along each direction.
    int DieAreaOffsetX() const { return die_area_offset_x_; }
    int DieAreaOffsetY() const { return die_area_offset_y_; }

    // get all blocks
    std::vector<Block> &Blocks() { return blocks_; }

    // some blocks are imaginary (for example, fixed IOPINs), this function
    // returns the number of real blocks (for example, gates)
    int RealBlkCnt() const { return real_block_count_; }

    // get all well tap cells
    std::vector<Block> &WellTaps() { return welltaps_; }

    // get well tap cell name-id map
    std::unordered_map<std::string, int> &TapNameIdMap() {
        return tap_name_id_map_;
    };

    // get all iopins
    std::vector<IoPin> &IoPins() { return iopins_; }

    // get all nets
    std::vector<Net> &Nets() { return nets_; }

    void UpdateFanoutHisto(int net_size);
    void InitNetFanoutHisto(std::vector<int> *histo_x = nullptr);
    void UpdateNetHPWLHisto(int net_size, double hpwl);
    void ReportNetFanoutHisto();
  private:
    /****design name****/
    std::string name_;

    /****def distance microns****/
    int distance_microns_ = 0;

    /****die area****/
    int region_left_ = 0; // unit is grid value x
    int region_right_ = 0; // unit is grid value x
    int region_bottom_ = 0; // unit is grid value y
    int region_top_ = 0; // unit is grid value y
    bool die_area_set_ = false;

    int die_area_offset_x_ = 0; // unit is manufacturing grid
    int die_area_offset_y_ = 0; // unit is manufacturing grid

    /****list of instances****/
    // block list consists of blocks and dummy blocks for pre-placed IOPINs
    std::vector<Block> blocks_;
    std::unordered_map<std::string, int> blk_name_id_map_;
    std::vector<Block> welltaps_;
    std::unordered_map<std::string, int> tap_name_id_map_;
    // number of blocks added by calling the AddBlock() API
    int real_block_count_ = 0;
    // number of blocks given in DEF, these two numbers are supposed to be the same
    int blk_count_limit_ = 0;

    /****list of IO Pins****/
    std::vector<IoPin> iopins_;
    std::unordered_map<std::string, int> iopin_name_id_map_;
    int pre_placed_io_count_ = 0;
    int added_iopin_count_ = 0;
    int iopin_count_limit_ = 0;

    /****list of nets****/
    double reset_signal_weight_ = 1;
    double normal_signal_weight_ = 1;
    std::vector<Net> nets_;
    int added_net_count_ = 0;
    int net_count_limit_ = 0;
    std::unordered_map<std::string, int> net_name_id_map_;
    NetHistogram net_histogram_;

    /****statistical data of the circuit****/
    long int tot_width_ = 0;
    long int tot_height_ = 0;
    long int tot_blk_area_ = 0;
    long int tot_mov_width_ = 0;
    long int tot_mov_height_ = 0;
    long int tot_mov_block_area_ = 0;
    int tot_mov_blk_num_ = 0;
    int blk_min_width_ = INT_MAX;
    int blk_max_width_ = INT_MIN;
    int blk_min_height_ = INT_MAX;
    int blk_max_height_ = INT_MIN;
};

}

#endif //DALI_DALI_CIRCUIT_DESIGN_H_
