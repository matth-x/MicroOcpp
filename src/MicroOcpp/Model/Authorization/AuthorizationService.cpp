// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Operations/SendLocalList.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Debug.h>

#define MOCPP_LOCALAUTHORIZATIONLIST_FN (MOCPP_FILENAME_PREFIX "localauth.jsn")

using namespace MicroOcpp;

AuthorizationService::AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {
    
    localAuthorizeOffline = declareConfiguration<bool>("LocalAuthorizeOffline", true, CONFIGURATION_FN, true, true, true, false);
    localAuthListEnabled = declareConfiguration<bool>("LocalAuthListEnabled", true, CONFIGURATION_FN, true, true, true, false);
    declareConfiguration<int>("LocalAuthListMaxLength", MOCPP_LocalAuthListMaxLength, CONFIGURATION_VOLATILE, false, true, false, false);
    declareConfiguration<int>("SendLocalListMaxLength", MOCPP_SendLocalListMaxLength, CONFIGURATION_VOLATILE, false, true, false, false);

    if (!localAuthorizeOffline || !localAuthListEnabled) {
        MOCPP_DBG_ERR("initialization error");
    }

    //append "LocalAuthListManagement" to FeatureProfiles list
    const char *fpId = "LocalAuthListManagement";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        *fProfile = fProfilePlus.c_str();
    }

    context.getOperationRegistry().registerOperation("GetLocalListVersion", [&context] () {
        return new Ocpp16::GetLocalListVersion(context.getModel());});
    context.getOperationRegistry().registerOperation("SendLocalList", [&context] () {
        return new Ocpp16::SendLocalList(context.getModel());});

    loadLists();
}

AuthorizationService::~AuthorizationService() {
    
}

bool AuthorizationService::loadLists() {
    if (!filesystem) {
        MOCPP_DBG_WARN("no fs access");
        return true;
    }

    size_t msize = 0;
    if (filesystem->stat(MOCPP_LOCALAUTHORIZATIONLIST_FN, &msize) != 0) {
        MOCPP_DBG_DEBUG("no local authorization list stored already");
        return true;
    }
    
    auto doc = FilesystemUtils::loadJson(filesystem, MOCPP_LOCALAUTHORIZATIONLIST_FN);
    if (!doc) {
        MOCPP_DBG_ERR("failed to load %s", MOCPP_LOCALAUTHORIZATIONLIST_FN);
        return false;
    }

    JsonObject root = doc->as<JsonObject>();

    if (!localAuthorizationList.readJson(root, true)) {
        MOCPP_DBG_ERR("list read failure");
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

    //check status
    if (authData->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
        MOCPP_DBG_DEBUG("idTag %s local auth status %s", idTag, serializeAuthorizationStatus(authData->getAuthorizationStatus()));
        return authData;
    }

    return authData;
}

bool AuthorizationService::updateLocalList(JsonObject payload) {
    bool success = localAuthorizationList.readJson(payload);

    if (success) {
        
        DynamicJsonDocument doc (localAuthorizationList.getJsonCapacity());

        JsonObject root = doc.to<JsonObject>();
        localAuthorizationList.writeJson(root, true);
        success = FilesystemUtils::storeJson(filesystem, MOCPP_LOCALAUTHORIZATIONLIST_FN, doc);

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
        auto& t_now = context.getModel().getClock().now();
        if (t_now > *localInfo->getExpiryDate()) {
            MOCPP_DBG_DEBUG("local auth expired");
            localStatus = AuthorizationStatus::Expired;
        }
    }

    bool equivalent = true;

    if (incomingStatus != localStatus) {
        MOCPP_DBG_WARN("local auth list status conflict");
        equivalent = false;
    }

    //check if parentIdTag definitions mismatch
    if (equivalent &&
            strcmp(localInfo->getParentIdTag() ? localInfo->getParentIdTag() : "", idTagInfo["parentIdTag"] | "")) {
        MOCPP_DBG_WARN("local auth list parentIdTag conflict");
        equivalent = false;
    }

    MOCPP_DBG_DEBUG("idTag %s fully evaluated: %s conflict", idTag, equivalent ? "no" : "contains");

    if (!equivalent) {
        //send error code "LocalListConflict" to server

        ChargePointStatus cpStatus = ChargePointStatus::NOT_SET;
        if (context.getModel().getNumConnectors() > 0) {
            cpStatus = context.getModel().getConnector(0)->getStatus();
        }

        auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                    0,
                    cpStatus, //will be determined in StatusNotification::initiate
                    context.getModel().getClock().now(),
                    "LocalListConflict"));

        statusNotification->setTimeout(60000);

        context.initiateRequest(std::move(statusNotification));
    }
}
