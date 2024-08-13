// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZATIONSERVICE_H
#define MO_AUTHORIZATIONSERVICE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;

class AuthorizationService : public MemoryManaged {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;
    AuthorizationList localAuthorizationList;

    std::shared_ptr<Configuration> localAuthListEnabledBool;

public:
    AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem);
    ~AuthorizationService();

    bool loadLists();

    AuthorizationData *getLocalAuthorization(const char *idTag);

    int getLocalListVersion();
    size_t getLocalListSize(); //number of entries in current localAuthList; used in unit tests

    bool updateLocalList(JsonArray localAuthorizationListJson, int listVersion, bool differential);

    void notifyAuthorization(const char *idTag, JsonObject idTagInfo);
};

}

#endif //MO_ENABLE_LOCAL_AUTH
#endif
