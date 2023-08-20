// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/FirmwareStatusNotification.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

using MicroOcpp::Ocpp16::FirmwareStatusNotification;

FirmwareStatusNotification::FirmwareStatusNotification(FirmwareStatus status) : status{status} {

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

std::unique_ptr<DynamicJsonDocument> FirmwareStatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = cstrFromFwStatus(status);
    return doc;
}

void FirmwareStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
