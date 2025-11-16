// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_PERSISTENCY_UTILS_H
#define MO_PERSISTENCY_UTILS_H

#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

#define MO_BOOTSTATS_VERSION_SIZE 10

#ifdef __cplusplus

namespace MicroOcpp {
namespace PersistencyUtils {

struct BootStats {
    uint16_t bootNr = 0;
    uint16_t attempts = 0; //incremented by 1 for each attempt to initialize the library, reset to 0 after successful initialization

    char microOcppVersion [MO_BOOTSTATS_VERSION_SIZE] = {'\0'};
};

bool loadBootStats(MO_FilesystemAdapter *filesystem, BootStats& bstats);
bool storeBootStats(MO_FilesystemAdapter *filesystem, BootStats& bstats);

bool migrate(MO_FilesystemAdapter *filesystem); //migrate persistent storage if running on a new MO version

//check for multiple boot failures in a row and if detected, delete all persistent files which could cause a crash. Call
//before MO is initialized / set up / as early as possible
bool autoRecovery(MO_FilesystemAdapter *filesystem);

bool setBootSuccess(MO_FilesystemAdapter *filesystem); //reset boot failure counter after successful boot

} //namespace PersistencyUtils
} //namespace MicroOcpp
#endif //__cplusplus
#endif
