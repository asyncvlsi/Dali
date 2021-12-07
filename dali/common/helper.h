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
#ifndef DALI_DALI_COMMON_HELPER_H_
#define DALI_DALI_COMMON_HELPER_H_

#include <string>
#include <vector>

#include "misc.h"

namespace dali {

// custom residual function, return: |x - round(x/y) * y|
double AbsResidual(double x, double y);

// splits a line into many words
void StrTokenize(std::string const &line, std::vector<std::string> &res);

// finds the first number in a string
int FindFirstNumber(std::string const &str);

// check if an executable can be found or not
bool IsExecutableExisting(std::string const &executable_path);

void ReportMemory();

void MergeIntervals(std::vector<SegI> &intervals);

}

#endif //DALI_DALI_COMMON_HELPER_H_
