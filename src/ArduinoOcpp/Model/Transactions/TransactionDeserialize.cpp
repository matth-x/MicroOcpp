// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <limits>

#include <ArduinoOcpp/Model/Transactions/TransactionDeserialize.h>
#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {

const char *serializeStopTxReason(StopTxReason reason) {
    const char *res = "Local";

    switch (reason) {
        case StopTxReason::DeAuthorized:
            res = "DeAuthorized";
            break;
        case StopTxReason::EmergencyStop:
            res = "EmergencyStop";
            break;
        case StopTxReason::EVDisconnected:
            res = "EVDisconnected";
            break;
        case StopTxReason::HardReset:
            res = "HardReset";
            break;
        case StopTxReason::Local:
            res = "Local";
            break;
        case StopTxReason::Other:
            res = "Other";
            break;
        case StopTxReason::PowerLoss:
            res = "PowerLoss";
            break;
        case StopTxReason::Reboot:
            res = "Reboot";
            break;
        case StopTxReason::Remote:
            res = "Remote";
            break;
        case StopTxReason::SoftReset:
            res = "SoftReset";
            break;
        case StopTxReason::UnlockCommand:
            res = "UnlockCommand";
            break;
    }

    return res;
}

bool deserializeStopTxReason(const char *reason_cstr, StopTxReason& out) {
    if (!strcmp(reason_cstr, "DeAuthorized")) {
        out = StopTxReason::DeAuthorized;
    } else if (!strcmp(reason_cstr, "EmergencyStop")) {
        out = StopTxReason::EmergencyStop;
    } else if (!strcmp(reason_cstr, "EVDisconnected")) {
        out = StopTxReason::EVDisconnected;
    } else if (!strcmp(reason_cstr, "HardReset")) {
        out = StopTxReason::HardReset;
    } else if (!strcmp(reason_cstr, "Local")) {
        out = StopTxReason::Local;
    } else if (!strcmp(reason_cstr, "Other")) {
        out = StopTxReason::Other;
    } else if (!strcmp(reason_cstr, "PowerLoss")) {
        out = StopTxReason::PowerLoss;
    } else if (!strcmp(reason_cstr, "Reboot")) {
        out = StopTxReason::Reboot;
    } else if (!strcmp(reason_cstr, "Remote")) {
        out = StopTxReason::Remote;
    } else if (!strcmp(reason_cstr, "SoftReset")) {
        out = StopTxReason::SoftReset;
    } else if (!strcmp(reason_cstr, "UnlockCommand")) {
        out = StopTxReason::UnlockCommand;
    } else {
        return false;
    }

    return true;
}

bool serializeSendStatus(SendStatus& status, JsonObject out) {
    if (status.isRequested()) {
        out["requested"] = true;
    }
    if (status.isConfirmed()) {
        out["confirmed"] = true;
    }
    return true;
}

bool deserializeSendStatus(SendStatus& status, JsonObject in) {
    if (in["requested"] | false) {
        status.setRequested();
    }
    if (in["confirmed"] | false) {
        status.confirm();
    }
    return true;
}

bool serializeTransaction(Transaction& tx, DynamicJsonDocument& out) {
    out = DynamicJsonDocument(1024);
    JsonObject state = out.to<JsonObject>();

    JsonObject sessionState = state.createNestedObject("session");
    if (!tx.isActive()) {
        sessionState["active"] = false;
    }
    if (tx.getIdTag()[0] != '\0') {
        sessionState["idTag"] = tx.getIdTag();
    }
    if (tx.isAuthorized()) {
        sessionState["authorized"] = true;
    }
    if (tx.isIdTagDeauthorized()) {
        sessionState["deauthorized"] = true;
    }
    if (tx.getBeginTimestamp() > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        tx.getBeginTimestamp().toJsonString(timeStr, JSONDATE_LENGTH + 1);
        sessionState["timestamp"] = timeStr;
    }
    if (tx.getReservationId() >= 0) {
        sessionState["reservationId"] = tx.getReservationId();
    }
    if (tx.getTxProfileId() >= 0) {
        sessionState["txProfileId"] = tx.getTxProfileId();
    }

    JsonObject txStart = state.createNestedObject("start");

    if (!serializeSendStatus(tx.getStartSync(), txStart)) {
        return false;
    }

    if (tx.isMeterStartDefined()) {
        txStart["meter"] = tx.getMeterStart();
    }

    char startTimeStr [JSONDATE_LENGTH + 1] = {'\0'};
    tx.getStartTimestamp().toJsonString(startTimeStr, JSONDATE_LENGTH + 1);
    txStart["timestamp"] = startTimeStr;

    txStart["bootNr"] = tx.getStartBootNr();

    if (tx.getStartSync().isConfirmed()) {
        txStart["transactionId"] = tx.getTransactionId();
    }

    JsonObject txStop = state.createNestedObject("stop");

    if (!serializeSendStatus(tx.getStopSync(), txStop)) {
        return false;
    }

    if (tx.getStopIdTag()[0] != '\0') {
        txStop["idTag"] = tx.getStopIdTag();
    }

    if (tx.isMeterStopDefined()) {
        txStop["meter"] = tx.getMeterStop();
    }

    char stopTimeStr [JSONDATE_LENGTH + 1] = {'\0'};
    tx.getStopTimestamp().toJsonString(stopTimeStr, JSONDATE_LENGTH + 1);
    txStop["timestamp"] = stopTimeStr;
    
    txStop["bootNr"] = tx.getStopBootNr();

    if (tx.getStopReason() != StopTxReason::Local) {
        txStop["reason"] = serializeStopTxReason(tx.getStopReason());
    }

    if (tx.isSilent()) {
        state["silent"] = true;
    }

    if (out.overflowed()) {
        AO_DBG_ERR("JSON capacity exceeded");
        return false;
    }

    return true;
}

bool deserializeTransaction(Transaction& tx, JsonObject state) {

    JsonObject sessionState = state["session"];

    if (!(sessionState["active"] | true)) {
        tx.setInactive();
    }

    if (sessionState.containsKey("idTag")) {
        if (!tx.setIdTag(sessionState["idTag"] | "")) {
            AO_DBG_ERR("read err");
            return false;
        }
    }

    if (sessionState["authorized"] | false) {
        tx.setAuthorized();
    }

    if (sessionState["deauthorized"] | false) {
        tx.setIdTagDeauthorized();
    }

    if (sessionState.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!timestamp.setTime(sessionState["timestamp"] | "Invalid")) {
            AO_DBG_ERR("read err");
            return false;
        }
        tx.setBeginTimestamp(timestamp);
    }

    if (sessionState.containsKey("reservationId")) {
        tx.setReservationId(sessionState["reservationId"] | -1);
    }

    if (sessionState.containsKey("txProfileId")) {
        tx.setTxProfileId(sessionState["txProfileId"] | -1);
    }

    JsonObject txStart = state["start"];

    if (!deserializeSendStatus(tx.getStartSync(), txStart)) {
        return false;
    }

    if (txStart.containsKey("meter")) {
        tx.setMeterStart(txStart["meter"] | 0);
    }

    if (txStart.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!timestamp.setTime(txStart["timestamp"] | "Invalid")) {
            AO_DBG_ERR("read err");
            return false;
        }
        tx.setStartTimestamp(timestamp);
    }

    if (txStart.containsKey("bootNr")) {
        int bootNrIn = txStart["bootNr"];
        if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
            tx.setStartBootNr((uint16_t) bootNrIn);
        } else {
            AO_DBG_ERR("read err");
            return false;
        }
    }

    if (txStart.containsKey("transactionId")) {
        tx.setTransactionId(txStart["transactionId"] | -1);
    }

    JsonObject txStop = state["stop"];

    if (!deserializeSendStatus(tx.getStopSync(), txStop)) {
        return false;
    }

    if (txStop.containsKey("idTag")) {
        if (!tx.setStopIdTag(txStop["idTag"] | "")) {
            AO_DBG_ERR("read err");
            return false;
        }
    }

    if (txStop.containsKey("meter")) {
        tx.setMeterStop(txStop["meter"] | 0);
    }

    if (txStop.containsKey("timestamp")) {
        Timestamp timestamp;
        if (!timestamp.setTime(txStop["timestamp"] | "Invalid")) {
            AO_DBG_ERR("read err");
            return false;
        }
        tx.setStopTimestamp(timestamp);
    }

    if (txStop.containsKey("bootNr")) {
        int bootNrIn = txStop["bootNr"];
        if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
            tx.setStopBootNr((uint16_t) bootNrIn);
        } else {
            AO_DBG_ERR("read err");
            return false;
        }
    }

    if (txStop.containsKey("reason")) {
        StopTxReason mreason = StopTxReason::Local;
        if (!deserializeStopTxReason(txStop["reason"] | "_Undefined", mreason)) {
            AO_DBG_ERR("read err");
            return false;
        }
        tx.setStopReason(mreason);
    }

    if (state["silent"] | false) {
        tx.setSilent();
    }

    AO_DBG_DEBUG("DUMP TX");
    AO_DBG_DEBUG("Session   | idTag %s, active: %i, authorized: %i, deauthorized: %i", tx.getIdTag(), tx.isActive(), tx.isAuthorized(), tx.isIdTagDeauthorized());
    AO_DBG_DEBUG("Start RPC | req: %i, conf: %i", tx.getStartSync().isRequested(), tx.getStartSync().isConfirmed());
    AO_DBG_DEBUG("Stop  RPC | req: %i, conf: %i",  tx.getStopSync().isRequested(), tx.getStopSync().isConfirmed());
    if (tx.isSilent()) {
        AO_DBG_DEBUG("          | silent Tx");
        (void)0;
    }

    return true;
}

}
