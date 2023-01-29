// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Debug.h>

#define AO_LOCALAUTHORIZATIONLIST_FN (AO_FILENAME_PREFIX "/localauth.jsn")

using namespace ArduinoOcpp;

AuthorizationService::AuthorizationService(OcppEngine& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {
    
    localAuthorizeOffline = declareConfiguration<bool>("LocalAuthorizeOffline", true, CONFIGURATION_FN, true, true, true, false);
    localAuthListEnabled = declareConfiguration<bool>("LocalAuthListEnabled", true, CONFIGURATION_FN, true, true, true, false);
    declareConfiguration<int>("LocalAuthListMaxLength", AO_LocalAuthListMaxLength, CONFIGURATION_VOLATILE, false, true, false, false);
    declareConfiguration<int>("SendLocalListMaxLength", AO_SendLocalListMaxLength, CONFIGURATION_VOLATILE, false, true, false, false);

    if (!localAuthorizeOffline || !localAuthListEnabled) {
        AO_DBG_ERR("initialization error");
    }

    //append "FirmwareManagement" to FeatureProfiles list
    const char *fpId = "FirmwareManagement";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }

    loadLists();
}

AuthorizationService::~AuthorizationService() {
    
}

bool AuthorizationService::loadLists() {
    if (!filesystem) {
        AO_DBG_WARN("no fs access");
        return false;
    }
    
    auto doc = FilesystemUtils::loadJson(filesystem, AO_LOCALAUTHORIZATIONLIST_FN);
    if (!doc) {
        AO_DBG_ERR("failed to load %s", AO_LOCALAUTHORIZATIONLIST_FN);
        return false;
    }

    JsonObject root = doc->as<JsonObject>();

    if (!localAuthorizationList.readJson(root, true)) {
        AO_DBG_ERR("list read failure");
        return false;
    }

    return true;
}

AuthorizationData *AuthorizationService::getLocalAuthorization(const char *idTag) {
    if (!*localAuthorizeOffline) {
        AO_DBG_DEBUG("skip local auth");
        return nullptr;
    }

    if (!*localAuthListEnabled) {
        AO_DBG_DEBUG("skip local auth list");
        return nullptr; //auth cache will follow
    }

    auto authData = localAuthorizationList.get(idTag);
    if (!authData) {
        AO_DBG_DEBUG("idTag %s not listed", idTag);
        return nullptr;
    }

    //check expiry
    if (authData->getExpiryDate()) {
        auto& tnow = context.getOcppModel().getOcppTime().getOcppTimestampNow();
        if (tnow >= *authData->getExpiryDate()) {
            AO_DBG_DEBUG("idTag %s local auth entry expired", idTag);
            return nullptr;
        }
    }

    //check status
    if (authData->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
        AO_DBG_DEBUG("idTag %s local auth status %s", idTag, serializeAuthorizationStatus(authData->getAuthorizationStatus()));
        return nullptr;
    }

    return authData;
}

bool AuthorizationService::updateLocalList(JsonObject payload) {
    bool success = localAuthorizationList.readJson(payload);

    if (success) {
        
        DynamicJsonDocument doc (localAuthorizationList.getJsonCapacity());

        JsonObject root = doc.to<JsonObject>();
        localAuthorizationList.writeJson(root, true);
        success = FilesystemUtils::storeJson(filesystem, AO_LOCALAUTHORIZATIONLIST_FN, doc);
    }

    return success;
}
