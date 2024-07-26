// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FIRMWARE_STATUS
#define MO_FIRMWARE_STATUS

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
