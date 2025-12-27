// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_VERSION_H
#define MO_VERSION_H

/*
 * Version specification of MicroOcpp library (not related with the OCPP version)
 */
#define MO_VERSION "2.0.0"

/*
 * OCPP version identifiers
 */
#define MO_OCPP_V16  160 // OCPP 1.6
#define MO_OCPP_V201 201 // OCPP 2.0.1

/*
 * Enable OCPP 1.6 support. If multiple OCPP versions are enabled, the final choice
 * of the protocol can be done dynamically during setup
 */
#ifndef MO_ENABLE_V16
#define MO_ENABLE_V16 1
#endif

/*
 * Enable OCPP 2.0.1 support. If multiple OCPP versions are enabled, the final choice
 * of the protocol can be done dynamically during setup
 */
#ifndef MO_ENABLE_V201
#define MO_ENABLE_V201 1
#endif

/*
 * Enable internal mock server. When MO uses the Loopback connection, or is connected
 * to an echo WS server, then MO can respond to its own requests and mock real OCPP
 * communication. This is useful for development and testing.
 */
#ifndef MO_ENABLE_MOCK_SERVER
#define MO_ENABLE_MOCK_SERVER 1
#endif

// Certificate Management (UCs M03 - M05). Works with OCPP 1.6 and 2.0.1
#ifndef MO_ENABLE_CERT_MGMT
#define MO_ENABLE_CERT_MGMT MO_ENABLE_V201
#endif

// Security Event log (UC A04). Works with OCPP 1.6 and 2.0.1
#ifndef MO_ENABLE_SECURITY_EVENT
#define MO_ENABLE_SECURITY_EVENT MO_ENABLE_V201
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

#ifndef MO_ENABLE_DIAGNOSTICS
#define MO_ENABLE_DIAGNOSTICS 1
#endif

#ifndef MO_ENABLE_CALIFORNIA
#define MO_ENABLE_CALIFORNIA 0
#endif

#endif
