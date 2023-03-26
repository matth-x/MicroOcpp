// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/MessagesV16/GetLocalListVersion.h>
#include <ArduinoOcpp/MessagesV16/SendLocalList.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
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

    //append "LocalAuthListManagement" to FeatureProfiles list
    const char *fpId = "LocalAuthListManagement";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }

    context.getOperationDeserializer().registerOcppOperation("GetLocalListVersion", [&context] () {
        return new Ocpp16::GetLocalListVersion(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("SendLocalList", [&context] () {
        return new Ocpp16::SendLocalList(context.getOcppModel());});

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
        return nullptr;
    }

    if (!*localAuthListEnabled) {
        return nullptr; //auth cache will follow
    }

    auto authData = localAuthorizationList.get(idTag);
    if (!authData) {
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

        if (!success) {
            loadLists();
        }
    }

    return success;
}

void AuthorizationService::notifyAuthorization(const char *idTag, JsonObject idTagInfo) {
    //check local list conflicts. In future: also update authorization cache

    if (!*localAuthorizeOffline) {
        return;
    }

    if (!*localAuthListEnabled) {
        return; //auth cache will follow
    }

    if (!idTagInfo.containsKey("status")) {
        return; //empty idTagInfo
    }

    auto localInfo = localAuthorizationList.get(idTag);
    if (!localInfo) {
        return;
    }

    //check for conflicts

    auto incomingStatus = deserializeAuthorizationStatus(idTagInfo["status"]);
    auto localStatus = localInfo->getAuthorizationStatus();

    if (incomingStatus == AuthorizationStatus::UNDEFINED) { //ignore invalid messages (handled elsewhere)
        return;
    }

    if (incomingStatus == AuthorizationStatus::ConcurrentTx) { //incoming status ConcurrentTx is equivalent to local Accepted
        incomingStatus = AuthorizationStatus::Accepted;
    }

    if (localStatus == AuthorizationStatus::Accepted && localInfo->getExpiryDate()) { //check for expiry
        auto& t_now = context.getOcppModel().getOcppTime().getOcppTimestampNow();
        if (t_now > *localInfo->getExpiryDate()) {
            AO_DBG_DEBUG("local auth expired");
            localStatus = AuthorizationStatus::Expired;
        }
    }

    bool equivalent = true;

    if (incomingStatus != localStatus) {
        AO_DBG_WARN("local auth list status conflict");
        equivalent = false;
    }

    //check if parentIdTag definitions mismatch
    if (equivalent &&
            strcmp(localInfo->getParentIdTag() ? localInfo->getParentIdTag() : "", idTagInfo["parentIdTag"] | "")) {
        AO_DBG_WARN("local auth list parentIdTag conflict");
        equivalent = false;
    }

    AO_DBG_DEBUG("idTag %s fully evaluated: %s conflict", idTag, equivalent ? "no" : "contains");

    if (!equivalent) {
        //send error code "LocalListConflict" to server

        OcppEvseState cpStatus;
        if (auto cp = context.getOcppModel().getConnectorStatus(0)) {
            cpStatus = cp->inferenceStatus();
        }

        auto statusNotification = makeOcppOperation(new Ocpp16::StatusNotification(
                    0,
                    cpStatus, //will be determined in StatusNotification::initiate
                    context.getOcppModel().getOcppTime().getOcppTimestampNow(),
                    "LocalListConflict"));

        statusNotification->setTimeout(60000);

        context.initiateOperation(std::move(statusNotification));
    }
}
