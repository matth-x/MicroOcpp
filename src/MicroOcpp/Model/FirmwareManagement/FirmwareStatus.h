// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef FIRMWARE_STATUS
#define FIRMWARE_STATUS

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

}
} //end namespace MicroOcpp
#endif
