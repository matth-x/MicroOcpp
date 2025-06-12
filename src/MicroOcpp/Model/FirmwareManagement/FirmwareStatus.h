// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FIRMWARE_STATUS
#define MO_FIRMWARE_STATUS

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

namespace MicroOcpp {
namespace Ocpp16 {

enum class FirmwareStatus {
    Downloaded,
    DownloadFailed,
    Downloading,
    Idle,
    InstallationFailed,
    Installing,
    Installed
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
#endif
