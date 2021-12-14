// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef FIRMWARE_STATUS
#define FIRMWARE_STATUS

namespace ArduinoOcpp {
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
} //end namespace ArduinoOcpp
#endif
