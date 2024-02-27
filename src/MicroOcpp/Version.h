// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_VERSION_H
#define MO_VERSION_H

/*
 * Version specification of MicroOcpp library (unrelated with the OCPP version)
 */
#define MO_VERSION "1.0.0"

/*
 * Enable OCPP 2.0.1 support. If enabled, library can be initialized with both v1.6 and v2.0.1. The choice
 * of the protocol is done dynamically during initialization
 */
#ifndef MO_ENABLE_V201
#define MO_ENABLE_V201 0
#endif

namespace MicroOcpp {

/*
 * OCPP version type, defined in Model
 */
struct ProtocolVersion {
    const int major, minor, patch;
    ProtocolVersion(int major = 1, int minor = 6, int patch = 0) : major(major), minor(minor), patch(patch) { }
};

}

#endif
