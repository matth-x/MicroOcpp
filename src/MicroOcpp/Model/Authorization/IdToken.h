// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_IDTOKEN_H
#define MO_IDTOKEN_H

#include <stdint.h>

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

#define MO_IDTAG_LEN_MAX 20

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#ifdef __cplusplus
extern "C" {
#endif

// IdTokenEnumType (3.43)
typedef enum {
    MO_IdTokenType_UNDEFINED,
    MO_IdTokenType_Central,
    MO_IdTokenType_eMAID,
    MO_IdTokenType_ISO14443,
    MO_IdTokenType_ISO15693,
    MO_IdTokenType_KeyCode,
    MO_IdTokenType_Local,
    MO_IdTokenType_MacAddress,
    MO_IdTokenType_NoAuthorization,
}   MO_IdTokenType;

#ifdef __cplusplus
} //extern "C"

#define MO_IDTOKEN_LEN_MAX 36

namespace MicroOcpp {
namespace v201 {

// IdTokenType (2.28)
class IdToken : public MemoryManaged {
private:
    char idToken [MO_IDTOKEN_LEN_MAX + 1];
    MO_IdTokenType type = MO_IdTokenType_UNDEFINED;
public:
    IdToken(const char *token = nullptr, MO_IdTokenType type = MO_IdTokenType_ISO14443, const char *memoryTag = nullptr);

    IdToken(const IdToken& other, const char *memoryTag = nullptr);

    IdToken& operator=(const IdToken& other);

    bool parseCstr(const char *token, const char *typeCstr);

    const char *get() const;
    MO_IdTokenType getType() const;
    const char *getTypeCstr() const;

    bool equals(const IdToken& other);
};

} //namespace MicroOcpp
} //namespace v201
#endif //__cplusplus
#endif //MO_ENABLE_V201
#endif
