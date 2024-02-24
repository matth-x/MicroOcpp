// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/SetVariables.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::SetVariables;

SetVariables::SetVariables(VariableService& variableService) : variableService(variableService) {
  
}

const char* SetVariables::getOperationType(){
    return "SetVariables";
}

void SetVariables::processReq(JsonObject payload) {
    for (JsonObject setVariable : payload["setVariableData"].as<JsonArray>()) {

        queries.emplace_back();
        auto& data = queries.back();

        if (setVariable.containsKey("attributeType")) {
            const char *attributeTypeCstr = setVariable["attributeType"] | "_Undefined";
            if (!strcmp(attributeTypeCstr, "Actual")) {
                data.attributeType = Variable::AttributeType::Actual;
            } else if (!strcmp(attributeTypeCstr, "Target")) {
                data.attributeType = Variable::AttributeType::Target;
            } else if (!strcmp(attributeTypeCstr, "MinSet")) {
                data.attributeType = Variable::AttributeType::MinSet;
            } else if (!strcmp(attributeTypeCstr, "MaxSet")) {
                data.attributeType = Variable::AttributeType::MaxSet;
            } else {
                errorCode = "FormationViolation";
                MO_DBG_ERR("invalid attributeType");
                return;
            }
        }

        const char *attributeValueCstr = setVariable["attributeValue"] | (const char*) nullptr;
        const char *componentNameCstr = setVariable["component"]["name"] | (const char*) nullptr;
        const char *variableNameCstr = setVariable["variable"]["name"] | (const char*) nullptr;

        if (!attributeValueCstr ||
                !componentNameCstr ||
                !variableNameCstr) {
            errorCode = "FormationViolation";
            return;
        }

        data.attributeValue = attributeValueCstr;
        data.componentName = componentNameCstr;
        data.variableName = variableNameCstr;

        // TODO check against ConfigurationValueSize

        data.componentEvseId = setVariable["component"]["evse"]["id"] | -1;
        data.componentEvseConnectorId = setVariable["component"]["evse"]["connectorId"] | -1;

        if (setVariable["component"].containsKey("evse") && data.componentEvseId < 0) {
            errorCode = "FormationViolation";
            MO_DBG_ERR("malformatted / missing evseId");
            return;
        }
    }

    if (queries.empty()) {
        errorCode = "FormationViolation";
        return;
    }

    MO_DBG_DEBUG("processing %zu setVariable queries", queries.size());

    for (auto query : queries) {
        query.attributeStatus = variableService.setVariable(
                query.attributeType,
                query.attributeValue,
                ComponentId(query.componentName.c_str(), 
                    EvseId(query.componentEvseId, query.componentEvseConnectorId)),
                query.variableName.c_str());
    }

    variableService.commit();
}

std::unique_ptr<DynamicJsonDocument> SetVariables::createConf(){
    size_t capacity = JSON_ARRAY_SIZE(queries.size());
    for (const auto& data : queries) {
        capacity += 
            JSON_OBJECT_SIZE(5) + // setVariableResult
                JSON_OBJECT_SIZE(2) + // component
                    data.componentName.length() + 1 +
                    JSON_OBJECT_SIZE(2) + // evse
                JSON_OBJECT_SIZE(2) + // variable
                    data.variableName.length() + 1;
    }
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));

    JsonObject payload = doc->to<JsonObject>();
    JsonArray setVariableResult = payload["setVariableResult"];

    for (const auto& data : queries) {
        JsonObject setVariable = setVariableResult.add();

        const char *attributeTypeCstr = nullptr;
        switch (data.attributeType) {
            case Variable::AttributeType::Actual:
                // leave blank when Actual
                break;
            case Variable::AttributeType::Target:
                attributeTypeCstr = "Target";
                break;
            case Variable::AttributeType::MinSet:
                attributeTypeCstr = "MinSet";
                break;
            case Variable::AttributeType::MaxSet:
                attributeTypeCstr = "MaxSet";
                break;
            default:
                MO_DBG_ERR("internal error");
                break;
        }
        if (attributeTypeCstr) {
            setVariable["attributeType"] = attributeTypeCstr;
        }

        const char *attributeStatusCstr = "Rejected";
        switch (data.attributeStatus) {
            case SetVariableStatus::Accepted:
                attributeStatusCstr = "Accepted";
                break;
            case SetVariableStatus::Rejected:
                attributeStatusCstr = "Rejected";
                break;
            case SetVariableStatus::UnknownComponent:
                attributeStatusCstr = "UnknownComponent";
                break;
            case SetVariableStatus::UnknownVariable:
                attributeStatusCstr = "UnknownVariable";
                break;
            case SetVariableStatus::NotSupportedAttributeType:
                attributeStatusCstr = "NotSupportedAttributeType";
                break;
            case SetVariableStatus::RebootRequired:
                attributeStatusCstr = "RebootRequired";
                break;
            default:
                MO_DBG_ERR("internal error");
                break;
        }
        setVariable["attributeStatus"] = attributeStatusCstr;

        setVariable["component"]["name"] = (char*) data.componentName.c_str(); // force copy-mode

        if (data.componentEvseId >= 0) {
            setVariable["component"]["evse"]["id"] = data.componentEvseId;
        }

        if (data.componentEvseConnectorId >= 0) {
            setVariable["component"]["evse"]["connectorId"] = data.componentEvseConnectorId;
        }

        setVariable["variable"]["name"] = (char*) data.variableName.c_str(); // force copy-mode
    }

    return doc;
}

#endif // MO_ENABLE_V201
