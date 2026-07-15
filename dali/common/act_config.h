/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
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
#ifndef DALI_COMMON_ACT_CONFIG_H_
#define DALI_COMMON_ACT_CONFIG_H_

// ACT's config header declares config_dump(FILE*) without including stdio.h.
// Keep the required include order local to this compatibility header.
// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on

#endif  // DALI_COMMON_ACT_CONFIG_H_
