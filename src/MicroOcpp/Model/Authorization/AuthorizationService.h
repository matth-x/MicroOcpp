// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONSERVICE_H
#define AUTHORIZATIONSERVICE_H

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Configuration.h>

namespace MicroOcpp {

class Context;

class AuthorizationService {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;
    AuthorizationList localAuthorizationList;

    std::shared_ptr<Configuration> localAuthorizeOfflineBool;
    std::shared_ptr<Configuration> localAuthListEnabledBool;

public:
    AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem);
    ~AuthorizationService();

    bool loadLists();

    AuthorizationData *getLocalAuthorization(const char *idTag);

    int getLocalListVersion() {return localAuthorizationList.getListVersion();}

    bool updateLocalList(JsonObject payload);

    void notifyAuthorization(const char *idTag, JsonObject idTagInfo);
};

}

#endif
