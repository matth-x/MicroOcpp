// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef DIAGNOSTICS_STATUS
#define DIAGNOSTICS_STATUS

namespace MicroOcpp {
namespace Ocpp16 {

enum class DiagnosticsStatus {
    Idle,
    Uploaded,
    UploadFailed,
    Uploading
};

}
} //end namespace MicroOcpp
#endif
