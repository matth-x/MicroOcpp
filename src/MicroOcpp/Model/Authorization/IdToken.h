// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_IDTOKEN_H
#define MO_IDTOKEN_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <stdint.h>

#define MO_IDTOKEN_LEN_MAX 36

namespace MicroOcpp {

// IdTokenType (2.28)
class IdToken {
public:

    // IdTokenEnumType (3.43)
    enum class Type : uint8_t {
        Central,
        eMAID,
        ISO14443,
        ISO15693,
        KeyCode,
        Local,
        MacAddress,
        NoAuthorization,
        UNDEFINED
    };

private:
    char idToken [MO_IDTOKEN_LEN_MAX + 1];
    Type type = Type::UNDEFINED;
public:
    IdToken();
    IdToken(const char *token, Type type = Type::ISO14443);

    bool parseCstr(const char *token, const char *typeCstr);

    const char *get() const;
    const char *getTypeCstr() const;

    bool equals(const IdToken& other);
};

} // namespace MicroOcpp

#endif // MO_ENABLE_V201
#endif
