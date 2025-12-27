// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef CALIFORNIAUTILS_H
#define CALIFORNIAUTILS_H

#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA
namespace MicroOcpp {

class Context;

namespace v16 {

class Transaction;
struct CaliforniaPrice;

namespace CaliforniaUtils {
bool validateDefaultPrice(const char* defaultPrice, void *userPtr);
bool validateTimeOffset(const char* timeOffset, void *userPtr);
bool validateNextTimeOffsetTransitionDateTime(const char* nextTimeOffsetTransitionDateTime, void *userPtr);
bool validateSupportLanguages(const char* supportLanguages, void *userPtr);
} //namespace CaliforniaUtils
} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif //MO_ENABLE_CALIFORNIA
#endif //CALIFORNIAUTILS_H