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

#define MO_LOCALAUTHORIZATIONLIST_FN (MO_FILENAME_PREFIX "localauth.jsn")

using namespace MicroOcpp;

AuthorizationService::AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {
    
    localAuthListEnabledBool = declareConfiguration<bool>("LocalAuthListEnabled", true);
    declareConfiguration<int>("LocalAuthListMaxLength", MO_LocalAuthListMaxLength, CONFIGURATION_VOLATILE, true);
    declareConfiguration<int>("SendLocalListMaxLength", MO_SendLocalListMaxLength, CONFIGURATION_VOLATILE, true);

    if (!localAuthListEnabledBool) {
        MO_DBG_ERR("initialization error");
    }
    
    context.getOperationRegistry().registerOperation("GetLocalListVersion", [&context] () {
        return new Ocpp16::GetLocalListVersion(context.getModel());});
    context.getOperationRegistry().registerOperation("SendLocalList", [this] () {
        return new Ocpp16::SendLocalList(*this);});

    loadLists();
}

AuthorizationService::~AuthorizationService() {
    
}

bool AuthorizationService::loadLists() {
    if (!filesystem) {
        MO_DBG_WARN("no fs access");
        return true;
    }

    size_t msize = 0;
    if (filesystem->stat(MO_LOCALAUTHORIZATIONLIST_FN, &msize) != 0) {
        MO_DBG_DEBUG("no local authorization list stored already");
        return true;
    }
    
    auto doc = FilesystemUtils::loadJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN);
    if (!doc) {
        MO_DBG_ERR("failed to load %s", MO_LOCALAUTHORIZATIONLIST_FN);
        return false;
    }

    JsonObject root = doc->as<JsonObject>();

    int listVersion = root["listVersion"] | 0;
    
    if (!localAuthorizationList.readJson(root["localAuthorizationList"].as<JsonArray>(), listVersion, false, true)) {
        MO_DBG_ERR("list read failure");
        return false;
    }

    return true;
}

AuthorizationData *AuthorizationService::getLocalAuthorization(const char *idTag) {
    if (!localAuthListEnabledBool->getBool()) {
        return nullptr; //auth cache will follow
    }

    auto authData = localAuthorizationList.get(idTag);
    if (!authData) {
        return nullptr;
    }

    //check status
    if (authData->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
        MO_DBG_DEBUG("idTag %s local auth status %s", idTag, serializeAuthorizationStatus(authData->getAuthorizationStatus()));
        return authData;
    }

    return authData;
}

int AuthorizationService::getLocalListVersion() {
    return localAuthorizationList.getListVersion();
}

size_t AuthorizationService::getLocalListSize() {
    return localAuthorizationList.size();
}

bool AuthorizationService::updateLocalList(JsonArray localAuthorizationListJson, int listVersion, bool differential) {
    bool success = localAuthorizationList.readJson(localAuthorizationListJson, listVersion, differential, false);

    if (success) {
        
        DynamicJsonDocument doc (
                JSON_OBJECT_SIZE(3) +
                localAuthorizationList.getJsonCapacity());

        JsonObject root = doc.to<JsonObject>();
        root["listVersion"] = listVersion;
        JsonArray authListCompact = root.createNestedArray("localAuthorizationList");
        localAuthorizationList.writeJson(authListCompact, true);
        success = FilesystemUtils::storeJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN, doc);

        if (!success) {
            loadLists();
        }
    }

    return success;
}

void AuthorizationService::notifyAuthorization(const char *idTag, JsonObject idTagInfo) {
    //check local list conflicts. In future: also update authorization cache

    if (!localAuthListEnabledBool->getBool()) {
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
            MO_DBG_DEBUG("local auth expired");
            localStatus = AuthorizationStatus::Expired;
        }
    }

    bool equivalent = true;

    if (incomingStatus != localStatus) {
        MO_DBG_WARN("local auth list status conflict");
        equivalent = false;
    }

    //check if parentIdTag definitions mismatch
    if (equivalent &&
            strcmp(localInfo->getParentIdTag() ? localInfo->getParentIdTag() : "", idTagInfo["parentIdTag"] | "")) {
        MO_DBG_WARN("local auth list parentIdTag conflict");
        equivalent = false;
    }

    MO_DBG_DEBUG("idTag %s fully evaluated: %s conflict", idTag, equivalent ? "no" : "contains");

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
