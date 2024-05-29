// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_VERSION_H
#define MO_VERSION_H

/*
 * Version specification of MicroOcpp library (not related with the OCPP version)
 */
#define MO_VERSION "1.2.0"

/*
 * Enable OCPP 2.0.1 support. If enabled, library can be initialized with both v1.6 and v2.0.1. The choice
 * of the protocol is done dynamically during initialization
 */
#ifndef MO_ENABLE_V201
#define MO_ENABLE_V201 0
#endif

#ifdef __cplusplus

namespace MicroOcpp {

/*
 * OCPP version type, defined in Model
 */
struct ProtocolVersion {
    const int major, minor, patch;
    ProtocolVersion(int major = 1, int minor = 6, int patch = 0) : major(major), minor(minor), patch(patch) { }
};

}

#endif //__cplusplus

// Certificate Management (UCs M03 - M05). Works with OCPP 1.6 and 2.0.1
#ifndef MO_ENABLE_CERT_MGMT
#define MO_ENABLE_CERT_MGMT MO_ENABLE_V201
#endif

// Reservations
#ifndef MO_ENABLE_RESERVATION
#define MO_ENABLE_RESERVATION 1
#endif

// Local Authorization, i.e. feature profile LocalAuthListManagement
#ifndef MO_ENABLE_LOCAL_AUTH
#define MO_ENABLE_LOCAL_AUTH 1
#endif

#endif
