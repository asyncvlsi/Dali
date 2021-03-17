//
// Created by Yihang Yang on 1/27/21.
//

#ifndef DALI_DALI_DALISCM_DALISCM_H_
#define DALI_DALI_DALISCM_DALISCM_H_

#include <cstdio>
#include <cstring>

#include <unordered_map>

#include <lisp.h>
#include <lispCli.h>

#include "dali/circuit.h"
#include "dali/placer.h"

namespace dali {

int process_initDali(int argc, char **argv);
int process_readLef(int argc, char **argv);
int process_readDef(int argc, char **argv);
int process_readCell(int argc, char **argv);
int process_loadConf(int argc, char **argv);
int process_setDensity(int argc, char **argv);
int process_globalPlace(int argc, char **argv);
int process_legalize(int argc, char **argv);
int process_wellLegalize(int argc, char **argv);
int process_savePlace(int argc, char **argv);
int process_closeDali(int argc, char **argv);

inline char kInitDaliStr[] = "initDali";
inline char kReadLefStr[] = "readLef";
inline char kReadDefStr[] = "readDef";
inline char kReadCellStr[] = "readCell";
inline char kLoadConfStr[] = "loadConf";
inline char kSetDensityStr[] = "setDensity";
inline char kGlobalPlaceStr[] = "globalPlace";
inline char kLegalizeStr[] = "legalize";
inline char kWellLegalizeStr[] = "wellLegalize";
inline char kSavePlaceStr[] = "savePlace";
inline char kCloseDaliStr[] = "closeDali";

inline struct LispCliCommand Cmds[] = {
    {nullptr, "Dali - Placement", nullptr},

    {kInitDaliStr, "initDali [-v verbosity_level(0-5)] - initialize dali", dali::process_initDali},
    {kReadLefStr, "readLef <lefFile> [-g grid_value_x, grid_value_y] - read LEF file", dali::process_readLef},
    {kReadDefStr, "readDef <defFile> - read DEF file", dali::process_readDef},
    {kReadCellStr, "readCell <cellFile> - read CELL file", dali::process_readCell},
    {kLoadConfStr, "loadConf <confFile> - load Dali placement configuration file", dali::process_loadConf},
    {kSetDensityStr, "setDensity <density> - set placement target density", dali::process_setDensity},
    {kGlobalPlaceStr, "globalPlace - start global placement", dali::process_globalPlace},
    {kLegalizeStr, "legalize - start legalization", dali::process_legalize},
    {kWellLegalizeStr, "wellLegalize - start well legalization", dali::process_wellLegalize},
    {kSavePlaceStr, "savePlace <outputDefFile> - save DEF file, only needs the base name", dali::process_savePlace},
    {kCloseDaliStr, "closeDali - close Dali and free memory", dali::process_closeDali}
};

}

#endif //DALI_DALI_DALISCM_DALISCM_H_
