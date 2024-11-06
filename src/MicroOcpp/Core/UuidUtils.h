#ifndef MO_UUIDUTILS_H
#define MO_UUIDUTILS_H

#include <stddef.h>
namespace MicroOcpp {

// Generates a UUID (Universally Unique Identifier) and writes it into a given buffer
// Returns false if the generation failed
// The buffer must be at least 37 bytes long (36 characters + zero termination)
bool generateUUID(char *uuidBuffer, size_t len);

}

#endif
