// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_AUTHORIZATIONLIST_H
#define MO_AUTHORIZATIONLIST_H

#include <MicroOcpp/Model/Authorization/AuthorizationData.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

#ifndef MO_LocalAuthListMaxLength
#define MO_LocalAuthListMaxLength 48
#endif

#ifndef MO_SendLocalListMaxLength
#define MO_SendLocalListMaxLength MO_LocalAuthListMaxLength
#endif

namespace MicroOcpp {

class Clock;

namespace Ocpp16 {

class AuthorizationList : public MemoryManaged {
private:
    int listVersion = 0;
    AuthorizationData **localAuthorizationList = nullptr; //sorted array
    size_t localAuthorizationListSize = 0;
public:
    AuthorizationList();
    ~AuthorizationList();

    AuthorizationData *get(const char *idTag);

    bool readJson(Clock& clock, JsonArray localAuthorizationList, int listVersion, bool differential, bool internalFormat); //internalFormat: if true, then use compact non-ocpp representation
    void clear();

    size_t getJsonCapacity(bool internalFormat);
    void writeJson(Clock& clock, JsonArray authListOut, bool internalFormat);

    int getListVersion() {return listVersion;}
    size_t size(); //used in unit tests

};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
#endif
