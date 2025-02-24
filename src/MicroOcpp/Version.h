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
 * OCPP version identifiers
 */
#define MO_OCPP_V16  160 // OCPP 1.6
#define MO_OCPP_V201 201 // OCPP 2.0.1

/*
 * Enable OCPP 2.0.1 support. If enabled, library can be initialized with v1.6 or v2.0.1. The choice
 * of the protocol is done dynamically during initialization
 */
#ifndef MO_ENABLE_V201
#define MO_ENABLE_V201 1
#endif

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

#ifndef MO_ENABLE_SMARTCHARGING
#define MO_ENABLE_SMARTCHARGING 1
#endif

#ifndef MO_ENABLE_FIRMWAREMANAGEMENT
#define MO_ENABLE_FIRMWAREMANAGEMENT 1
#endif

#endif
