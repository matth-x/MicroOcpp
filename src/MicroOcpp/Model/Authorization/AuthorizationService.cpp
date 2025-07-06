// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Operations/SendLocalList.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

#define MO_LOCALAUTHORIZATIONLIST_FN "localauth.jsn"

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

AuthorizationService::AuthorizationService(Context& context) : MemoryManaged("v16.Authorization.AuthorizationService"), context(context) {

}

bool AuthorizationService::setup() {

    filesystem = context.getFilesystem();

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    localAuthListEnabledBool = configService->declareConfiguration<bool>("LocalAuthListEnabled", true);
    if (!localAuthListEnabledBool) {
        MO_DBG_ERR("initialization error");
        return false;
    }

    configService->declareConfiguration<int>("LocalAuthListMaxLength", MO_LocalAuthListMaxLength, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
    configService->declareConfiguration<int>("SendLocalListMaxLength", MO_SendLocalListMaxLength, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);

    context.getMessageService().registerOperation("GetLocalListVersion", [] (Context& context) -> Operation* {
        return new GetLocalListVersion(context.getModel16());});
    context.getMessageService().registerOperation("SendLocalList", [] (Context& context) -> Operation* {
        return new SendLocalList(*context.getModel16().getAuthorizationService());});

    return loadLists();
}

bool AuthorizationService::loadLists() {
    if (!filesystem) {
        MO_DBG_WARN("no fs access");
        return true;
    }

    JsonDoc doc (0);
    auto ret = FilesystemUtils::loadJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN, doc, getMemoryTag());
    switch (ret) {
        case FilesystemUtils::LoadStatus::Success:
            break; //continue loading JSON
        case FilesystemUtils::LoadStatus::FileNotFound:
            MO_DBG_DEBUG("no local authorization list stored already");
            return true;
        case FilesystemUtils::LoadStatus::ErrOOM:
            MO_DBG_ERR("OOM");
            return false;
        case FilesystemUtils::LoadStatus::ErrFileCorruption:
        case FilesystemUtils::LoadStatus::ErrOther:
            MO_DBG_ERR("failed to load %s", MO_LOCALAUTHORIZATIONLIST_FN);
            return false;
    }

    int listVersion = doc["listVersion"] | 0;

    if (!localAuthorizationList.readJson(context.getClock(), doc["localAuthorizationList"].as<JsonArray>(), listVersion, /*differential*/ false, /*internalFormat*/ true)) {
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
    bool success = localAuthorizationList.readJson(context.getClock(), localAuthorizationListJson, listVersion, differential, /*internalFormat*/ false);

    if (success && filesystem) {

        auto doc = initJsonDoc(getMemoryTag(),
                JSON_OBJECT_SIZE(3) +
                localAuthorizationList.getJsonCapacity(/*internalFormat*/ true));

        JsonObject root = doc.to<JsonObject>();
        root["listVersion"] = listVersion;
        JsonArray authListCompact = root.createNestedArray("localAuthorizationList");
        localAuthorizationList.writeJson(context.getClock(), authListCompact, /*internalFormat*/ true);
        success = (FilesystemUtils::storeJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN, doc) == FilesystemUtils::StoreStatus::Success);

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
        int32_t dtExpiryDate;
        if (context.getClock().delta(context.getClock().now(), *localInfo->getExpiryDate(), dtExpiryDate)) {
            if (dtExpiryDate > 0) {
                //now is past expiryDate
                MO_DBG_DEBUG("local auth expired");
                localStatus = AuthorizationStatus::Expired;
            }
        } else {
            MO_DBG_ERR("cannot determine local auth expiry");
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

        auto availSvc = context.getModel16().getAvailabilityService();
        auto availSvcCp = availSvc ? availSvc->getEvse(0) : nullptr;
        auto cpStatus = availSvcCp ? availSvcCp->getStatus() : MO_ChargePointStatus_UNDEFINED;

        MO_ErrorData errorCode;
        mo_ErrorData_init(&errorCode);
        mo_ErrorData_setErrorCode(&errorCode, "LocalListConflict");

        auto statusNotification = makeRequest(context, new StatusNotification(
                    context,
                    0,
                    cpStatus,
                    context.getClock().now(),
                    errorCode));

        statusNotification->setTimeout(60);

        context.getMessageService().sendRequest(std::move(statusNotification));
    }
}

#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
