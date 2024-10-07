// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHANGEAVAILABILITYSTATUS_H
#define MO_CHANGEAVAILABILITYSTATUS_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <stdint.h>

namespace MicroOcpp {

enum class ChangeAvailabilityStatus : uint8_t {
    Accepted,
    Rejected,
    Scheduled
};

} //namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
