//
// Created by Yihang Yang on 12/17/19.
//

#ifndef DALI_SRC_OPENDB_H_
#define DALI_SRC_OPENDB_H_

#include "db.h"
#include "lefin.h"
#include "defin.h"
#include "lefout.h"
#include "defout.h"
#include <vector>

// the following functions come from OpenDB/src/swig/python/dbhelpers.i without any modification

odb::dbLib* odb_read_lef(odb::dbDatabase* db, const char* path);

std::vector<odb::dbLib*> odb_read_lef(odb::dbDatabase* db, std::vector<std::string> paths);

odb::dbChip* odb_read_def(std::vector<odb::dbLib*>& libs, std::vector<std::string> paths);

odb::dbChip* odb_read_def(odb::dbDatabase* db, std::vector<std::string> paths);

odb::dbChip* odb_read_design(odb::dbDatabase* db, std::vector<std::string> &lef_path, std::vector<std::string> def_path);

odb::dbChip* odb_read_design(odb::dbDatabase* db, std::vector<std::string> def_path);

int odb_write_def(odb::dbBlock* block, const char* path, odb::defout::Version version = odb::defout::Version::DEF_5_5);

int odb_write_lef(odb::dbLib* lib, const char* path);

#endif //DALI_SRC_OPENDB_H_
