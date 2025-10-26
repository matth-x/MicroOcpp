// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_MUTABILITY_H
#define MO_MUTABILITY_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

#ifdef __cplusplus
extern "C" {
#endif

//MutabilityEnumType (3.58), shared between Configuration and Variable
typedef enum {
    MO_Mutability_ReadWrite,
    MO_Mutability_ReadOnly,
    MO_Mutability_WriteOnly,
    MO_Mutability_None
}   MO_Mutability;

#ifdef __cplusplus
} //extern "C"

namespace MicroOcpp {

//MutabilityEnumType (3.58), shared between Configuration and Variable
enum class Mutability : uint8_t {
    ReadWrite,
    ReadOnly,
    WriteOnly,
    None
};

} //namespace MicroOcpp

#endif //__cplusplus
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif //MO_MUTABILITY_H
