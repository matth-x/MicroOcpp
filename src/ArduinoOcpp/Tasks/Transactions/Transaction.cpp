// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

bool TransactionRPC::serializeSessionState(JsonObject rpc) {
    rpc["requested"] = requested;
    rpc["confirmed"] = confirmed;
    return true;
}

bool Transaction::serializeSessionState(DynamicJsonDocument& out) {
    out = DynamicJsonDocument(1024);
    JsonObject state = out.to<JsonObject>();

    JsonObject sessionState = state.createNestedObject("session");
    if (session.idTag[0] != '\0') {
        sessionState["idTag"] = session.idTag;
    }
    if (session.authorized) {
        sessionState["authorized"] = session.authorized;
    }
    if (session.timestamp > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        session.timestamp.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        sessionState["timestamp"] = timeStr;
    }
    if (session.txProfileId >= 0) {
        sessionState["txProfileId"] = session.txProfileId;
    }
    if (!session.active) {
        sessionState["active"] = session.active;
    }

    JsonObject txStart = state.createNestedObject("start");

    JsonObject txStartRPC = txStart.createNestedObject("rpc");
    if (!start.rpc.serializeSessionState(txStartRPC)) {
        return false;
    }

    JsonObject txStartClientSide = txStart.createNestedObject("client");

    if (start.client.timestamp > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        start.client.timestamp.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txStartClientSide["timestamp"] = timeStr;
    }

    if (start.client.meter >= 0) {
        txStartClientSide["meter"] = start.client.meter;
    }

    if (start.client.reservationId >= 0) {
        txStartClientSide["reservationId"] = start.client.reservationId;
    }


    if (start.rpc.confirmed) {
        JsonObject txStartServerSide = txStart.createNestedObject("server");
        txStartServerSide["transactionId"] = start.server.transactionId;
        txStartServerSide["authorized"] = start.server.authorized;
    }

    JsonObject txStop = state.createNestedObject("stop");

    JsonObject txStopRPC = txStop.createNestedObject("rpc");
    if (!stop.rpc.serializeSessionState(txStopRPC)) {
        return false;
    }

    JsonObject txStopClientSide = txStop.createNestedObject("client");

    if (stop.client.timestamp > MIN_TIME) {
        char timeStr [JSONDATE_LENGTH + 1] = {'\0'};
        stop.client.timestamp.toJsonString(timeStr, JSONDATE_LENGTH + 1);
        txStopClientSide["timestamp"] = timeStr;
    }

    if (stop.client.meter >= 0) {
        txStopClientSide["meter"] = stop.client.meter;
    }

    if (stop.client.idTag[0] != '\0') {
        txStopClientSide["idTag"] = stop.client.idTag;
    }

    if (stop.client.reason[0] != '\0') {
        txStopClientSide["reason"] = stop.client.reason;
    }

    if (silent) {
        state["silent"] = true;
    }

    if (out.overflowed()) {
        AO_DBG_ERR("JSON capacity exceeded");
        return false;
    }

    return true;
}

bool TransactionRPC::deserializeSessionState(JsonObject rpc) {
    if (rpc["requested"] | false) {
        requested = true;
    }
    if (rpc["confirmed"] | false) {
        confirmed = true;
    }
    return true;
}

bool Transaction::deserializeSessionState(JsonObject state) {

    JsonObject sessionState = state["session"];

    if (sessionState.containsKey("idTag")) {
        if (snprintf(session.idTag, sizeof(session.idTag), "%s", sessionState["idTag"] | "") < 0) {
            AO_DBG_ERR("Read err");
            return false;
        }
    }
    if (sessionState.containsKey("authorized")) {
        session.authorized = sessionState["authorized"] | false;
    }
    if (sessionState.containsKey("timestamp")) {
        session.timestamp.setTime(sessionState["timestamp"] | "Invalid");
    }
    if (sessionState.containsKey("txProfileId")) {
        session.txProfileId = sessionState["txProfileId"] | -1;
    }
    if (sessionState.containsKey("active")) {
        session.active = sessionState["active"] | true;
    }

    JsonObject txStart = state["start"];

    if (txStart.containsKey("rpc")) {
        JsonObject txStartRPC = txStart["rpc"];
        if (!start.rpc.deserializeSessionState(txStartRPC)) {
            return false;
        }
    }

    JsonObject txStartClientSide = txStart["client"];

    if (txStartClientSide.containsKey("timestamp")) {
        start.client.timestamp.setTime(txStartClientSide["timestamp"] | "Invalid");
    }

    if (txStartClientSide.containsKey("meter")) {
        start.client.meter = txStartClientSide["meter"] | 0;
    }

    if (txStartClientSide.containsKey("reservationId")) {
        start.client.reservationId = txStartClientSide["reservationId"] | -1;
    }

    if (start.rpc.confirmed) {
        JsonObject txStartServerSide = txStart["server"];
        start.server.transactionId = txStartServerSide["transactionId"] | -1;
        start.server.authorized = txStartServerSide["authorized"] | false;
    }

    JsonObject txStop = state["stop"];

    if (txStop.containsKey("rpc")) {
        JsonObject txStopRPC = txStop["rpc"];
        if (!stop.rpc.deserializeSessionState(txStopRPC)) {
            return false;
        }
    }

    JsonObject txStopClientSide = txStop["client"];

    if (txStopClientSide.containsKey("timestamp")) {
        stop.client.timestamp.setTime(txStopClientSide["timestamp"] | "Invalid");
    }

    if (txStopClientSide.containsKey("meter")) {
        stop.client.meter = txStopClientSide["meter"] | 0;
    }

    if (txStopClientSide.containsKey("idTag")) {
        if (snprintf(stop.client.idTag, sizeof(stop.client.idTag), "%s", txStopClientSide["idTag"] | "") < 0) {
            AO_DBG_ERR("Read err");
            return false;
        }
    }

    if (txStopClientSide.containsKey("reason")) {
        if (snprintf(stop.client.reason, sizeof(stop.client.reason), "%s", txStopClientSide["reason"] | "") < 0) {
            AO_DBG_ERR("Read err");
            return false;
        }
    }

    if (state.containsKey("silent")) {
        silent = state["silent"] | false;
    }

    AO_DBG_DEBUG("DUMP TX");
    AO_DBG_DEBUG("Session   | idTag %s", session.idTag);
    AO_DBG_DEBUG("Start RPC | req: %i, conf: %i", start.rpc.requested, start.rpc.confirmed);
    AO_DBG_DEBUG("Stop  RPC | req: %i, conf: %i",  stop.rpc.requested, stop.rpc.confirmed);
    if (silent) {
        AO_DBG_DEBUG("          | silent Tx");
        (void)0;
    }

    return true;
}

bool Transaction::commit() {
    return context.commit(this);
}
