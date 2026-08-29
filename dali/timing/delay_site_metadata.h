/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/

/** @file Declared, adjustable timing-delay sites. */
#ifndef DALI_TIMING_DELAY_SITE_METADATA_H_
#define DALI_TIMING_DELAY_SITE_METADATA_H_

#include <string>
#include <vector>

namespace dali {

/**
 * One adjustable delay meta component declared by the design producer.
 *
 * `logical_path_prefix` identifies the part of logical timing witnesses owned
 * by the site. When it is omitted, `instance_name` is used. This keeps timing
 * repair independent of a particular synthesized instance-name convention.
 */
struct DelayRepairSite {
  std::string id;
  std::string process_name;
  std::string instance_name;
  std::string logical_path_prefix;
  std::string kind;
  std::string parameter_name;
  int initial_parameter_value = 0;
  bool adjustable = false;
};

/**
 * Load and validate schema-v1 `delay_sites` metadata.
 *
 * The metadata is produced with the ACT design and describes the source-level
 * meta components that Dali may name in an advisory timing-repair plan. The
 * output is changed only after the complete file validates.
 */
bool ReadDelayRepairSiteMetadata(const std::string &file_name,
                                 std::vector<DelayRepairSite> *sites,
                                 std::string *error_message);

} // namespace dali

#endif // DALI_TIMING_DELAY_SITE_METADATA_H_
