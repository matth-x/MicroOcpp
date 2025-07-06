// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetVariables.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Debug.h>

#include <ctype.h> //for tolower

#if MO_ENABLE_V201

using namespace MicroOcpp;
using namespace MicroOcpp::v201;

GetVariableData::GetVariableData(const char *memory_tag) : componentName{makeString(memory_tag)}, variableName{makeString(memory_tag)} {

}

GetVariables::GetVariables(VariableService& variableService) : MemoryManaged("v201.Operation.", "GetVariables"), variableService(variableService), queries(makeVector<GetVariableData>(getMemoryTag())) {

}

const char* GetVariables::getOperationType(){
    return "GetVariables";
}

void GetVariables::processReq(JsonObject payload) {
    for (JsonObject getVariable : payload["getVariableData"].as<JsonArray>()) {

        queries.emplace_back(getMemoryTag());
        auto& data = queries.back();

        if (getVariable.containsKey("attributeType")) {
            const char *attributeTypeCstr = getVariable["attributeType"] | "_Undefined";
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

        const char *componentNameCstr = getVariable["component"]["name"] | (const char*) nullptr;
        const char *variableNameCstr = getVariable["variable"]["name"] | (const char*) nullptr;

        if (!componentNameCstr ||
                !variableNameCstr) {
            errorCode = "FormationViolation";
            return;
        }

        data.componentName = componentNameCstr;
        data.variableName = variableNameCstr;

        // TODO check against ConfigurationValueSize

        data.componentEvseId = getVariable["component"]["evse"]["id"] | -1;
        data.componentEvseConnectorId = getVariable["component"]["evse"]["connectorId"] | -1;

        if (getVariable["component"].containsKey("evse") && data.componentEvseId < 0) {
            errorCode = "FormationViolation";
            MO_DBG_ERR("malformatted / missing evseId");
            return;
        }
    }

    if (queries.empty()) {
        errorCode = "FormationViolation";
        return;
    }
}

std::unique_ptr<JsonDoc> GetVariables::createConf(){

    // process GetVariables queries
    for (auto& query : queries) {
        query.attributeStatus = variableService.getVariable(
                query.attributeType,
                ComponentId(query.componentName.c_str(), 
                    EvseId(query.componentEvseId, query.componentEvseConnectorId)),
                query.variableName.c_str(),
                &query.variable);
    }

    #define VALUE_BUFSIZE 30 // for primitives (int)

    size_t capacity = JSON_ARRAY_SIZE(queries.size());
    for (const auto& data : queries) {
        size_t valueCapacity = 0;
        if (data.variable) {
            switch (data.variable->getInternalDataType()) {
                case Variable::InternalDataType::Int: {
                    // measure int size by printing to a dummy buf
                    char valbuf [VALUE_BUFSIZE];
                    auto ret = snprintf(valbuf, VALUE_BUFSIZE, "%i", data.variable->getInt());
                    if (ret < 0 || ret >= VALUE_BUFSIZE) {
                        continue;
                    }
                    valueCapacity = (size_t) ret + 1;
                    break;
                }
                case Variable::InternalDataType::Bool:
                    // bool will be stored in zero-copy mode (string literal "true" or "false")
                    valueCapacity = 0;
                    break;
                case Variable::InternalDataType::String:
                    valueCapacity = strlen(data.variable->getString()); // TODO limit by ReportingValueSize
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    break;
            }
        }

        capacity += 
            JSON_OBJECT_SIZE(5) + // getVariableResult
                valueCapacity + // capacity needed for storing the value
                JSON_OBJECT_SIZE(2) + // component
                    data.componentName.length() + 1 +
                    JSON_OBJECT_SIZE(2) + // evse
                JSON_OBJECT_SIZE(2) + // variable
                    data.variableName.length() + 1;
    }

    auto doc = makeJsonDoc(getMemoryTag(), capacity);

    JsonObject payload = doc->to<JsonObject>();
    JsonArray getVariableResult = payload.createNestedArray("getVariableResult");

    for (const auto& data : queries) {
        JsonObject getVariable = getVariableResult.createNestedObject();

        const char *attributeStatusCstr = "Rejected";
        switch (data.attributeStatus) {
            case GetVariableStatus::Accepted:
                attributeStatusCstr = "Accepted";
                break;
            case GetVariableStatus::Rejected:
                attributeStatusCstr = "Rejected";
                break;
            case GetVariableStatus::UnknownComponent:
                attributeStatusCstr = "UnknownComponent";
                break;
            case GetVariableStatus::UnknownVariable:
                attributeStatusCstr = "UnknownVariable";
                break;
            case GetVariableStatus::NotSupportedAttributeType:
                attributeStatusCstr = "NotSupportedAttributeType";
                break;
            default:
                MO_DBG_ERR("internal error");
                break;
        }
        getVariable["attributeStatus"] = attributeStatusCstr;

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
            getVariable["attributeType"] = attributeTypeCstr;
        }

        if (data.variable) {
            switch (data.variable->getInternalDataType()) {
                case Variable::InternalDataType::Int: {
                    char valbuf [VALUE_BUFSIZE];
                    auto ret = snprintf(valbuf, VALUE_BUFSIZE, "%i", data.variable->getInt());
                    if (ret < 0 || ret >= VALUE_BUFSIZE) {
                        break;
                    }
                    getVariable["attributeValue"] = valbuf;
                    break;
                }
                case Variable::InternalDataType::Bool:
                    getVariable["attributeValue"] = data.variable->getBool() ? "true" : "false";
                    break;
                case Variable::InternalDataType::String:
                    getVariable["attributeValue"] = (char*) data.variable->getString(); // force zero-copy mode
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    break;
            }
        }

        getVariable["component"]["name"] = (char*) data.componentName.c_str(); // force copy-mode

        if (data.componentEvseId >= 0) {
            getVariable["component"]["evse"]["id"] = data.componentEvseId;
        }

        if (data.componentEvseConnectorId >= 0) {
            getVariable["component"]["evse"]["connectorId"] = data.componentEvseConnectorId;
        }

        getVariable["variable"]["name"] = (char*) data.variableName.c_str(); // force copy-mode
    }

    return doc;
}

#endif //MO_ENABLE_V201
