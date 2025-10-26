#ifndef MO_UUIDUTILS_H
#define MO_UUIDUTILS_H

#include <stdint.h>
#include <stddef.h>

#define MO_UUID_STR_SIZE (36 + 1)

namespace MicroOcpp {
namespace UuidUtils {

// Generates a UUID (Universally Unique Identifier) and writes it into a given buffer
// Returns false if the generation failed
// The buffer must be at least 37 bytes long (36 characters + zero termination)
bool generateUUID(uint32_t (*rng)(), char *uuidBuffer, size_t size);

} //namespace UuidUtils
} //namespace MicroOcpp
#endif
