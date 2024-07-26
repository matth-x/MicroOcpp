// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DIAGNOSTICS_STATUS
#define MO_DIAGNOSTICS_STATUS

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
