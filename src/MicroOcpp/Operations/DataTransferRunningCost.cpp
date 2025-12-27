// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/California/CaliforniaService.h>
#include <MicroOcpp/Operations/DataTransferRunningCost.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Context.h>
#include <MicroOcpp/Debug.h>
#include <ArduinoJson.h>

#if MO_ENABLE_V16
#if MO_ENABLE_CALIFORNIA

using namespace MicroOcpp;

v16::DataTransferRunningCost::DataTransferRunningCost(Context& context) : 
    MemoryManaged("v16.Operation.", "DataTransferRunningCost"), context(context)
{
}

v16::DataTransferRunningCost::~DataTransferRunningCost() {
}

const char* v16::DataTransferRunningCost::getOperationType(){
    return "DataTransferRunningCost";
}

void v16::DataTransferRunningCost::processReq(JsonObject payload) {
    if (!context.getModel16().getCaliforniaService()->getCustomDisplayCostAndPrice()) {
        return;
    }
    
    if (!payload["data"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("data not string");
        return;
    }

    // parse data field
    size_t capacity = 512;
    DeserializationError err = DeserializationError::NoMemory;
    std::unique_ptr<MicroOcpp::JsonDoc> doc;
    while (err == DeserializationError::NoMemory && capacity < MO_MAX_JSON_CAPACITY) {
        doc = makeJsonDoc(getMemoryTag(), capacity);
        err = deserializeJson(*doc, payload["data"].as<const char*>());
        capacity *= 2;
    }
    if (err != DeserializationError::Ok) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("Failed to parse RunningCost data: %s", err.c_str());
        return;
    }


    if (!doc->containsKey("transactionId") || !doc->containsKey("timestamp") || 
            !doc->containsKey("cost") || !doc->containsKey("meterValue") ||
            !doc->containsKey("state") || !doc->containsKey("chargingPrice")) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("Missing required field");
        return;
    }

    auto data = doc->as<JsonObject>();
    if (!data["transactionId"].is<int>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("Invalid transactionId");
        return;
    }

    int transactionId = data["transactionId"].as<int>();

    auto& model = context.getModel16();

    TransactionServiceEvse *transactionServiceEvse = nullptr;
    CaliforniaServiceEvse  *californiaServiceEvse = nullptr;
    for (uint32_t evseId = 0; evseId < MO_NUM_EVSEID; evseId++) {
        transactionServiceEvse = model.getTransactionService()->getEvse(evseId);
        if (transactionServiceEvse && transactionServiceEvse->getTransaction() && 
            transactionServiceEvse->getTransaction()->getTransactionId() == transactionId) {
            californiaServiceEvse = model.getCaliforniaService()->getEvse(evseId);
            break;
        }
    }

    if (!californiaServiceEvse || !transactionServiceEvse) {
        errorCode = "InternalError";
        MO_DBG_DEBUG("transactionId not found");
        return;
    }

    const char *timestamp = data["timestamp"] | "";
    Timestamp ts;
    if (!context.getClock().parseString(timestamp, ts)) {
        errorCode = "InternalError";
        MO_DBG_DEBUG("Invalid timestamp");
        return;
    }

    if (!data["meterValue"].is<int>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("meterValue not integer");
        return;
    }
    double meterValue = data["meterValue"].as<int>();

    if (!data["cost"].is<double>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("cost not double");
        return;
    }
    double cost = data["cost"].as<double>();

    if (!data["state"].is<const char*>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("state not string");
        return;
    }
    const char* state = data["state"].as<const char*>();

    bool isCharging = false;
    if (!strcmp(state, "Charging") && !strcmp(state, "Idle")) {
        errorCode = "InternalError";
        MO_DBG_DEBUG("Invalid state");
        return;
    }
    else {
        isCharging = strcmp(state, "Charging") == 0;
    }

    if (!doc->operator[]("chargingPrice").is<JsonObject>()) {
        errorCode = "FormationViolation";
        MO_DBG_DEBUG("chargingPrice not object");
        return;
    }


    CaliforniaPrice price;
    price.isValid = true;
    price.isPricekWhPriceValid = false;
    price.isPriceHourPriceValid = false;
    price.isPriceFlatFeeValid = false;
    price.isIdleGraceMinutesValid = false;
    price.isIdleHourPriceValid = false;
    JsonObject chargingPrice = doc->operator[]("chargingPrice").as<JsonObject>();
    for (auto kvp : chargingPrice) {
        const char* key = kvp.key().c_str();
        if (strcmp(key, "kWhPrice") == 0) {
            if (!price.isPricekWhPriceValid) {
                price.isPricekWhPriceValid = true;
                price.chargingPricekWhPrice = kvp.value().as<double>();
            } else {
                errorCode = "FormationViolation";
                MO_DBG_DEBUG("Duplicate kWhPrice");
                return;
            }
        } else if (strcmp(key, "hourPrice") == 0) {
            if (!price.isPriceHourPriceValid) {
                price.isPriceHourPriceValid = true;
                price.chargingPriceHourPrice = kvp.value().as<double>();
            } else {
                errorCode = "FormationViolation";
                MO_DBG_DEBUG("Duplicate hourPrice");
                return;
            }
        } else if (strcmp(key, "flatFee") == 0) {
            if (!price.isPriceFlatFeeValid) {
                price.isPriceFlatFeeValid = true;
                price.chargingPriceFlatFee = kvp.value().as<double>();
            } else {
                errorCode = "FormationViolation";
                MO_DBG_DEBUG("Duplicate flatFee");
                return;
            }
        }
        else {
            errorCode = "FormationViolation";
            MO_DBG_DEBUG("Unknown chargingPrice key %s", key);
            return;
        }
    }

    if (data.containsKey("idlePrice")) {
        if (data["idlePrice"].is<JsonObject>()) {
            auto idlePrice = data["idlePrice"].as<JsonObject>();
            for (auto kvp : idlePrice) {
                const char* key = kvp.key().c_str();
                if (strcmp(key, "graceMinutes") == 0) {
                        if (!price.isIdleGraceMinutesValid) {
                        price.isIdleGraceMinutesValid = true;
                        price.idleGraceMinutes = kvp.value().as<int>();
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate idleGraceMinutes");
                        return;
                    }
                } else if (strcmp(key, "hourPrice") == 0) {
                    if (!price.isIdleHourPriceValid) {
                        price.isIdleHourPriceValid = true;
                        price.idleHourPrice = kvp.value().as<double>();
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate idleHourPrice");
                        return;
                    }
                } else {
                    errorCode = "FormationViolation";
                    MO_DBG_DEBUG("Unknown idlePrice key %s", key);
                    return;
                }
            }
        } else {
                errorCode = "FormationViolation";
                MO_DBG_DEBUG("idlePrice not object");
                return;
        }
    }

    
    CaliforniaPrice nextPeriod;
    nextPeriod.isValid = false;
    if (data.containsKey("nextPeriod")) {
        if (data["nextPeriod"].is<JsonObject>()) {
            auto nextPeriodObj = data["nextPeriod"].as<JsonObject>();
            nextPeriod.isValid = true;
            nextPeriod.isPricekWhPriceValid = false;
            nextPeriod.isPriceHourPriceValid = false;
            nextPeriod.isPriceFlatFeeValid = false;
            nextPeriod.isIdleGraceMinutesValid = false;
            nextPeriod.isIdleHourPriceValid = false;
            for (auto kvp : nextPeriodObj) {
                const char* key = kvp.key().c_str();
                if (strcmp(key, "atTime") == 0) {
                    if (!nextPeriod.isAtTimeValid) {
                        if (context.getClock().parseString(kvp.value(), nextPeriod.atTime)) {
                            nextPeriod.isAtTimeValid = true;
                        } else {
                            errorCode = "FormationViolation";
                            MO_DBG_DEBUG("Invalid atTime");
                            return;
                        }
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate atTime");
                        return;
                    }
                } else if (strcmp(key, "chargingPrice") == 0) {
                    for (auto ikvp : kvp.value().as<JsonObject>()) {
                        const char* key = ikvp.key().c_str();
                        if (strcmp(key, "kWhPrice") == 0) {
                            if (!nextPeriod.isPricekWhPriceValid) {
                                nextPeriod.isPricekWhPriceValid = true;
                                nextPeriod.chargingPricekWhPrice = ikvp.value().as<double>();
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Duplicate kWhPrice");
                                return;
                            }
                        } else if (strcmp(key, "hourPrice") == 0) {
                            if (!nextPeriod.isPriceHourPriceValid) {
                                nextPeriod.isPriceHourPriceValid = true;
                                nextPeriod.chargingPriceHourPrice = ikvp.value().as<double>();
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Duplicate hourPrice");
                                return;
                            }
                        } else if (strcmp(key, "flatFee") == 0) {
                            if (!nextPeriod.isPriceFlatFeeValid) {
                                nextPeriod.isPriceFlatFeeValid = true;
                                nextPeriod.chargingPriceFlatFee = ikvp.value().as<double>();
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Duplicate flatFee");
                                return;
                            }
                        }
                        else {
                            errorCode = "FormationViolation";
                            MO_DBG_DEBUG("Unknown chargingPrice key %s", key);
                            return;
                        }
                    }
                } else if (strcmp(key, "idlePrice") == 0) {
                    for (auto ikvp : kvp.value().as<JsonObject>()) {
                        const char* key = ikvp.key().c_str();
                        if (strcmp(key, "graceMinutes") == 0) {
                            if (!nextPeriod.isIdleGraceMinutesValid) {
                                nextPeriod.isIdleGraceMinutesValid = true;
                                nextPeriod.idleGraceMinutes = ikvp.value().as<int>();
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Duplicate idleGraceMinutes");
                                return;
                            }
                        } else if (strcmp(key, "hourPrice") == 0) {
                            if (!nextPeriod.isIdleHourPriceValid) {
                                nextPeriod.isIdleHourPriceValid = true;
                                nextPeriod.idleHourPrice = ikvp.value().as<double>();
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Duplicate idleHourPrice");
                                return;
                            }
                        }
                        else {
                            errorCode = "FormationViolation";
                            MO_DBG_DEBUG("Unknown idlePrice key %s", key);
                            return;
                        }
                    }
                } else {
                    errorCode = "FormationViolation";
                    MO_DBG_DEBUG("Unknown nextPeriod key %s", key);
                    return;
                }
            }
        } else {
            errorCode = "FormationViolation";
            MO_DBG_DEBUG("nextPeriod not object");
            return;
        }
    }

    CaliforniaTrigger trigger;
    trigger.isValid = false;
    if (data.containsKey("triggerMeterValue")) {
        if (data["triggerMeterValue"].is<JsonObject>()) {
            auto triggerMeterValue = data["triggerMeterValue"].as<JsonObject>();
            trigger.isValid = true;
            trigger.isAtEnergykWhValid = false;
            trigger.isAtPowerkWValid = false;
            trigger.isAtCPStatusValid = false;

            for (auto kvp : triggerMeterValue) {
                const char* key = kvp.key().c_str();
                if (strcmp(key, "atTime") == 0) {
                    if (!trigger.isAtTimeValid) {
                        if (context.getClock().parseString(kvp.value(), trigger.atTime)) {
                            trigger.isAtTimeValid = true;
                        } else {
                            errorCode = "FormationViolation";
                            MO_DBG_DEBUG("Invalid atTime");
                            return;
                        }
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate atTime");
                        return;
                    }
                } else if (strcmp(key, "atEnergykWh") == 0) {
                    if (!trigger.isAtEnergykWhValid) {
                        trigger.isAtEnergykWhValid = true;
                        trigger.atEnergykWh = kvp.value().as<int>();
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate atEnergykWh");
                        return;
                    }
                } else if (strcmp(key, "atPowerkW") == 0) {
                    if (!trigger.isAtPowerkWValid) {
                        trigger.isAtPowerkWValid = true;
                        trigger.atPowerkW = kvp.value().as<double>();
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate atPowerkW");
                        return;
                    }
                } else if (strcmp(key, "atCPStatus") == 0) {
                    if (!trigger.isAtCPStatusValid) {
                        trigger.isAtCPStatusValid = true;
                        auto statusArray = kvp.value().as<JsonArray>();
                        // initialize to UNDEFINED
                        for (size_t i = 0; i < sizeof(trigger.atCPStatus)/sizeof(trigger.atCPStatus[0]); i++)
                            trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_UNDEFINED;
                        size_t i = 0;
                        for (JsonVariant v : statusArray) {
                            if (i >= sizeof(trigger.atCPStatus)/sizeof(trigger.atCPStatus[0]))
                            {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Too many atCPStatus values");
                                return;
                            }

                            const char* statusStr = v.as<const char*>();
                            if (strcmp(statusStr, "Available") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_Available;
                            } else if (strcmp(statusStr, "Preparing") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_Preparing;
                            } else if (strcmp(statusStr, "Charging") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_Charging;
                            } else if (strcmp(statusStr, "SuspendedEV") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_SuspendedEV;
                            } else if (strcmp(statusStr, "SuspendedEVSE") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_SuspendedEVSE;
                            } else if (strcmp(statusStr, "Finishing") == 0) {
                                trigger.atCPStatus[i] = MO_ChargePointStatus::MO_ChargePointStatus_Finishing;
                            } else {
                                errorCode = "FormationViolation";
                                MO_DBG_DEBUG("Invalid atCPStatus value %s", statusStr);
                                return;
                            }
                        }
                    } else {
                        errorCode = "FormationViolation";
                        MO_DBG_DEBUG("Duplicate atCPStatus");
                        return;
                    }
                }
                else {
                    errorCode = "FormationViolation";
                    MO_DBG_DEBUG("Unknown triggerMeterValue key %s", key);
                    return;
                }
            }
        } else {
            errorCode = "FormationViolation";
            MO_DBG_DEBUG("triggerMeterValue not object");
            return;
        }
    }

    // for (auto transactionService : model)
    californiaServiceEvse->updateCost(cost, meterValue);
    californiaServiceEvse->updatePrice(price);
    californiaServiceEvse->updateChargingStatus(isCharging);
    if (nextPeriod.isValid) {
        californiaServiceEvse->updateNextPrice(nextPeriod);
    }
    if (trigger.isValid) {
        californiaServiceEvse->updateTrigger(trigger);
    }
}

std::unique_ptr<JsonDoc> v16::DataTransferRunningCost::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = "Rejected";
    return doc;
}

#endif //MO_ENABLE_CALIFORNIA
#endif //MO_ENABLE_V16
