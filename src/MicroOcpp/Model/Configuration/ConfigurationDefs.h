// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CONFIGURATIONDEFS_H
#define MO_CONFIGURATIONDEFS_H

#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) // Also enable in v201 to simplify backwards compatibility

#ifndef MO_CONFIG_EXT_PREFIX
#define MO_CONFIG_EXT_PREFIX "Cst_"
#endif

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201)
#endif
