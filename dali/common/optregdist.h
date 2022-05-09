/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

#ifndef DALI_COMMON_OPTREGDIST_H_
#define DALI_COMMON_OPTREGDIST_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "logging.h"

namespace dali {

// optimal region distance
class OptRegDist {
 public:
  Circuit *circuit_ = nullptr;
  void FindOptimalRegionX(
      Block &blk,
      double &lx,
      double &ly,
      double &ux,
      double &uy
  ) const;

  void SaveFile(std::string const &file_name) const;
};

}

#endif //DALI_COMMON_OPTREGDIST_H_
