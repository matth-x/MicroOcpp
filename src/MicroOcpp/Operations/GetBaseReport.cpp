// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetBaseReport.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

GetBaseReport::GetBaseReport(VariableService& variableService) : MemoryManaged("v201.Operation.", "GetBaseReport"), variableService(variableService) {
  
}

const char* GetBaseReport::getOperationType(){
    return "GetBaseReport";
}

void GetBaseReport::processReq(JsonObject payload) {

    int requestId = payload["requestId"] | -1;
    if (requestId < 0) {
        errorCode = "FormationViolation";
        MO_DBG_ERR("invalid requestId");
        return;
    }

    ReportBase reportBase;

    const char *reportBaseCstr = payload["reportBase"] | "";
    if (!strcmp(reportBaseCstr, "ConfigurationInventory")) {
        reportBase = ReportBase_ConfigurationInventory;
    } else if (!strcmp(reportBaseCstr, "FullInventory")) {
        reportBase = ReportBase_FullInventory;
    } else if (!strcmp(reportBaseCstr, "SummaryInventory")) {
        reportBase = ReportBase_SummaryInventory;
    } else {
        errorCode = "FormationViolation";
        MO_DBG_ERR("invalid reportBase");
        return;
    }

    status = variableService.getBaseReport(requestId, reportBase);
}

std::unique_ptr<JsonDoc> GetBaseReport::createConf(){

    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    const char *statusCstr = "";

    switch (status) {
        case GenericDeviceModelStatus_Accepted:
            statusCstr = "Accepted";
            break;
        case GenericDeviceModelStatus_Rejected:
            statusCstr = "Rejected";
            break;
        case GenericDeviceModelStatus_NotSupported:
            statusCstr = "NotSupported";
            break;
        case GenericDeviceModelStatus_EmptyResultSet:
            statusCstr = "EmptyResultSet";
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }

    payload["status"] = statusCstr;

    return doc;
}

#endif //MO_ENABLE_V201
