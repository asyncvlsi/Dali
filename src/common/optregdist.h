//
// Created by yihang on 9/9/20.
//

#ifndef DALI_SRC_COMMON_OPTREGDIST_H_
#define DALI_SRC_COMMON_OPTREGDIST_H_

#include <vector>

#include "circuit/circuit.h"

struct OptRegDist {
  Circuit *circuit_ = nullptr;
  void FindOptimalRegionX(Block &blk, double &lx, double &ly, double &ux, double &uy) const {
    std::vector<double> loc_list_x;
    std::vector<double> loc_list_y;
    auto &net_list = circuit_->getDesign()->net_list;
    for (auto &it: *(blk.NetList())) {
      // find offset
      double offset_x = DBL_MAX;
      double offset_y = DBL_MAX;
      for (auto &blk_pin: net_list[it].blk_pin_list) {
        if (blk_pin.BlkPtr() == &blk) {
          offset_x = blk_pin.OffsetX();
          offset_y = blk_pin.OffsetY();
          break;
        }
      }

      // find max/min x/y of this net without this block
      double min_x = 1e10;
      double max_x = -1e10;
      double min_y = 1e10;
      double max_y = -1e10;
      for (auto &blk_pin: net_list[it].blk_pin_list) {
        if (blk_pin.BlkPtr() == &blk) {
          continue;
        } else {
          if (blk_pin.AbsX() < min_x) {
            min_x = blk_pin.AbsX();
          }
          if (blk_pin.AbsX() > max_x) {
            max_x = blk_pin.AbsX();
          }
          if (blk_pin.AbsY() < min_y) {
            min_y = blk_pin.AbsY();
          }
          if (blk_pin.AbsY() > max_y) {
            max_y = blk_pin.AbsY();
          }
        }
      }
      loc_list_x.push_back(min_x - offset_x);
      loc_list_x.push_back(max_x - offset_x);
      loc_list_y.push_back(min_y - offset_y);
      loc_list_y.push_back(max_y - offset_y);
    }
    std::sort(loc_list_x.begin(), loc_list_x.end());
    std::sort(loc_list_y.begin(), loc_list_y.end());
    int lo_index = int(loc_list_x.size()-1)/2;
    int hi_index = lo_index;
    if (loc_list_x.size()%2==0) {
      hi_index += 1;
    }
    lx = loc_list_x[lo_index];
    ux = loc_list_x[hi_index];

    lo_index = int(loc_list_y.size()-1)/2;
    hi_index = lo_index;
    if (loc_list_y.size()%2==0) {
      hi_index += 1;
    }
    ly = loc_list_y[lo_index];
    uy = loc_list_y[hi_index];
  }

  void SaveFile(std::string &file_name) const {
    if (globalVerboseLevel >= LOG_CRITICAL) {
      printf("Writing optimal region distance file '%s', ", file_name.c_str());
    }
    std::ofstream ost(file_name.c_str());
    Assert(ost.is_open(), "Cannot open file " + file_name);

    if (circuit_ == nullptr) return;
    double ave_size = circuit_->AveBlkHeight();
    double lx = DBL_MAX;
    double ly = DBL_MAX;
    double ux = DBL_MAX;
    double uy = DBL_MAX;
    double llx, lly;
    double res;
    for (auto &blk: circuit_->getDesign()->block_list) {
      FindOptimalRegionX(blk, lx, ly, ux, uy);
      llx = blk.LLX();
      lly = blk.LLY();
      bool x_optimal = lx <= llx && llx <= ux;
      bool y_optimal = ly <= lly && lly <= uy;
      if (x_optimal && y_optimal) {
        res = 0;
      } else if (x_optimal) {
        res = std::min(std::fabs(llx - lx), std::fabs(llx - ux));
      } else if (y_optimal) {
        res = std::min(std::fabs(lly - ly), std::fabs(lly - uy));
      } else {
        double x_distance = std::min(std::fabs(llx - lx), std::fabs(llx - ux));
        double y_distance = std::min(std::fabs(lly - ly), std::fabs(lly - uy));
        res = std::sqrt(x_distance * x_distance + y_distance * y_distance);
      }
      ost << res/ave_size << "\n";
    }
    std::cout << "average cell width: " << ave_size << "\n";
  }
};

#endif //DALI_SRC_COMMON_OPTREGDIST_H_
