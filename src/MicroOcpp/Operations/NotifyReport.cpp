// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/NotifyReport.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp::Ocpp201;
using MicroOcpp::MemJsonDoc;

NotifyReport::NotifyReport(Model& model, int requestId, const Timestamp& generatedAt, bool tbc, int seqNo, const MemVector<Variable*>& reportData)
        : AllocOverrider("v201.Operation.", getOperationType()), model(model), requestId(requestId), generatedAt(generatedAt), tbc(tbc), seqNo(seqNo), reportData(reportData) {

}

const char* NotifyReport::getOperationType() {
    return "NotifyReport";
}

std::unique_ptr<MemJsonDoc> NotifyReport::createReq() {

    #define VALUE_BUFSIZE 30 // for primitives (int)

    const Variable::AttributeType enumerateAttributeTypes [] = {
        Variable::AttributeType::Actual,
        Variable::AttributeType::Target,
        Variable::AttributeType::MinSet,
        Variable::AttributeType::MaxSet
    };

    size_t capacity = 
            JSON_OBJECT_SIZE(5) + //total of 5 fields
            JSONDATE_LENGTH + 1; //timestamp string

    capacity += JSON_ARRAY_SIZE(reportData.size());
    for (auto variable : reportData) {
        capacity += JSON_OBJECT_SIZE(4); //total of 4 fields
        capacity += 2 * JSON_OBJECT_SIZE(2); //component composite
        capacity += JSON_OBJECT_SIZE(1); //variable composite

        size_t nAttributes = 0;
        size_t valueCapacity = 0;
        for (auto attributeType : enumerateAttributeTypes) {
            if (!variable->hasAttribute(attributeType)) {
                continue;
            }
            nAttributes++;
            switch (variable->getInternalDataType()) {
                case Variable::InternalDataType::Int: {
                    // measure int size by printing to a dummy buf
                    char valbuf [VALUE_BUFSIZE];
                    auto ret = snprintf(valbuf, VALUE_BUFSIZE, "%i", variable->getInt());
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
                    valueCapacity = strlen(variable->getString()); // TODO limit by ReportingValueSize
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    break;
            }
        }

        capacity += nAttributes * JSON_OBJECT_SIZE(5); //variableAttribute composite
        capacity += valueCapacity; //variableAttribute value total size

        capacity += JSON_OBJECT_SIZE(2); //variableCharacteristics composite: only send two data fields
    }

    auto doc = makeMemJsonDoc(capacity, getMemoryTag());

    JsonObject payload = doc->to<JsonObject>();
    payload["requestId"] = requestId;

    char generatedAtCstr [JSONDATE_LENGTH + 1];
    generatedAt.toJsonString(generatedAtCstr, sizeof(generatedAtCstr));
    payload["generatedAt"] = generatedAtCstr;

    if (tbc) {
        payload["tbc"] = true;
    }

    payload["seqNo"] = seqNo;

    JsonArray reportDataJsonArray = payload.createNestedArray("reportData");

    for (auto variable : reportData) {
        JsonObject reportDataJson = reportDataJsonArray.createNestedObject();

        reportDataJson["component"]["name"] = (char*) variable->getComponentId().name; // force copy-mode

        if (variable->getComponentId().evse.id >= 0) {
            reportDataJson["component"]["evse"]["id"] = variable->getComponentId().evse.id;
        }

        if (variable->getComponentId().evse.connectorId >= 0) {
            reportDataJson["component"]["evse"]["connectorId"] = variable->getComponentId().evse.connectorId;
        }

        reportDataJson["variable"]["name"] = (char*) variable->getName(); // force copy-mode

        JsonArray variableAttribute = reportDataJson.createNestedArray("variableAttribute");

        for (auto attributeType : enumerateAttributeTypes) {
            if (!variable->hasAttribute(attributeType)) {
                continue;
            }

            JsonObject attribute = variableAttribute.createNestedObject();

            const char *attributeTypeCstr = nullptr;
            switch (attributeType) {
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
                attribute["type"] = attributeTypeCstr;
            }

            if (variable->getMutability() != Variable::Mutability::WriteOnly) {
                switch (variable->getInternalDataType()) {
                    case Variable::InternalDataType::Int: {
                        char valbuf [VALUE_BUFSIZE];
                        auto ret = snprintf(valbuf, VALUE_BUFSIZE, "%i", variable->getInt());
                        if (ret < 0 || ret >= VALUE_BUFSIZE) {
                            break;
                        }
                        attribute["value"] = valbuf;
                        break;
                    }
                    case Variable::InternalDataType::Bool:
                        attribute["value"] = variable->getBool() ? "true" : "false";
                        break;
                    case Variable::InternalDataType::String:
                        attribute["value"] = (char*) variable->getString(); // force zero-copy mode
                        break;
                    default:
                        MO_DBG_ERR("internal error");
                        break;
                }
            }

            const char *mutabilityCstr = nullptr;
            switch (variable->getMutability()) {
                case Variable::Mutability::ReadOnly:
                    mutabilityCstr = "ReadOnly";
                    break;
                case Variable::Mutability::WriteOnly:
                    mutabilityCstr = "WriteOnly";
                    break;
                case Variable::Mutability::ReadWrite:
                    // leave blank when ReadWrite
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    break;
            }
            if (mutabilityCstr) {
                attribute["mutability"] = mutabilityCstr;
            }

            if (variable->isPersistent()) {
                attribute["persistent"] = true;
            }

            if (variable->isConstant()) {
                attribute["constant"] = true;
            }
        }

        JsonObject variableCharacteristics = reportDataJson.createNestedObject("variableCharacteristics");

        const char *dataTypeCstr = "";
        switch (variable->getVariableDataType()) {
            case VariableCharacteristics::DataType::string:
                dataTypeCstr = "string";
                break;
            case VariableCharacteristics::DataType::decimal:
                dataTypeCstr = "decimal";
                break;
            case VariableCharacteristics::DataType::integer:
                dataTypeCstr = "integer";
                break;
            case VariableCharacteristics::DataType::dateTime:
                dataTypeCstr = "dateTime";
                break;
            case VariableCharacteristics::DataType::boolean:
                dataTypeCstr = "boolean";
                break;
            case VariableCharacteristics::DataType::OptionList:
                dataTypeCstr = "OptionList";
                break;
            case VariableCharacteristics::DataType::SequenceList:
                dataTypeCstr = "SequenceList";
                break;
            case VariableCharacteristics::DataType::MemberList:
                dataTypeCstr = "MemberList";
                break;
            default:
                MO_DBG_ERR("internal error");
                break; 
        }
        variableCharacteristics["dataType"] = dataTypeCstr;

        variableCharacteristics["supportsMonitoring"] = variable->getSupportsMonitoring();
    }

    return doc;
}

void NotifyReport::processConf(JsonObject payload) {
    // empty payload
}

#endif // MO_ENABLE_V201
