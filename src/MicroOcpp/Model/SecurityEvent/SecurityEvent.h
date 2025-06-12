// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_SECURITYEVENTSERVICE_H
#define MO_SECURITYEVENTSERVICE_H

#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

// Length of field `type` of a SecurityEventNotificationRequest (1.49.1)
#define MO_SECURITY_EVENT_TYPE_LENGTH 50

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
#endif
