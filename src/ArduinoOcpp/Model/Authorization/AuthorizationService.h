// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONSERVICE_H
#define AUTHORIZATIONSERVICE_H

#include <ArduinoOcpp/Model/Authorization/AuthorizationList.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/Configuration.h>

namespace ArduinoOcpp {

class Context;

class AuthorizationService {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;
    AuthorizationList localAuthorizationList;

    std::shared_ptr<Configuration<bool>> localAuthorizeOffline;
    std::shared_ptr<Configuration<bool>> localAuthListEnabled;

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
