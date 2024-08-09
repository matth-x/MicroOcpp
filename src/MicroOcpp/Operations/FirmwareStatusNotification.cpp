// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/FirmwareStatusNotification.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

using MicroOcpp::Ocpp16::FirmwareStatusNotification;
using MicroOcpp::MemJsonDoc;

FirmwareStatusNotification::FirmwareStatusNotification(FirmwareStatus status) : AllocOverrider("v16.Operation.", getOperationType()), status{status} {

}

const char *FirmwareStatusNotification::cstrFromFwStatus(FirmwareStatus status) {
    switch (status) {
        case (FirmwareStatus::Downloaded):
            return "Downloaded";
            break;
        case (FirmwareStatus::DownloadFailed):
            return "DownloadFailed";
            break;
        case (FirmwareStatus::Downloading):
            return "Downloading";
            break;
        case (FirmwareStatus::Idle):
            return "Idle";
            break;
        case (FirmwareStatus::InstallationFailed):
            return "InstallationFailed";
            break;
        case (FirmwareStatus::Installing):
            return "Installing";
            break;
        case (FirmwareStatus::Installed):
            return "Installed";
            break;
    }
    return NULL; //cannot be reached
}

std::unique_ptr<MemJsonDoc> FirmwareStatusNotification::createReq() {
    auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(1), getMemoryTag());
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = cstrFromFwStatus(status);
    return doc;
}

void FirmwareStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
