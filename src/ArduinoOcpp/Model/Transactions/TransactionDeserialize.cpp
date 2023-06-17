// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/TransactionDeserialize.h>
#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {

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

    if (tx.getStartTimestamp() > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        tx.getStartTimestamp().toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txStart["timestamp"] = timeStr;
    }

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

    if (tx.isMeterStopDefined() >= 0) {
        txStop["meter"] = tx.getMeterStop();
    }

    if (tx.getStopTimestamp() > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        tx.getStopTimestamp().toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txStop["timestamp"] = timeStr;
    }

    if (tx.getStopReason()[0] != '\0') {
        txStop["reason"] = tx.getStopReason();
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
        if (!timestamp.setTime(sessionState["timestamp"] | "Invalid")) {
            AO_DBG_ERR("read err");
            return false;
        }
        tx.setStartTimestamp(timestamp);
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

    if (txStop.containsKey("reason")) {
        if (!tx.setStopReason(txStop["reason"] | "")) {
            AO_DBG_ERR("read err");
            return false;
        }
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
