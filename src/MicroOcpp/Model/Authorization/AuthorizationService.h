// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZATIONSERVICE_H
#define MO_AUTHORIZATIONSERVICE_H

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

#ifdef __cplusplus

namespace MicroOcpp {

class Context;

namespace v16 {

class Configuration;

class AuthorizationService : public MemoryManaged {
private:
    Context& context;
    MO_FilesystemAdapter *filesystem = nullptr;
    AuthorizationList localAuthorizationList;

    Configuration *localAuthListEnabledBool = nullptr;

public:
    AuthorizationService(Context& context);

    bool setup();

    bool loadLists();

    AuthorizationData *getLocalAuthorization(const char *idTag);

    int getLocalListVersion();
    size_t getLocalListSize(); //number of entries in current localAuthList; used in unit tests

    bool updateLocalList(JsonArray localAuthorizationListJson, int listVersion, bool differential);

    void notifyAuthorization(const char *idTag, JsonObject idTagInfo);
};

} //namespace MicroOcpp
} //namespace v16
#endif //__cplusplus
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
#endif
