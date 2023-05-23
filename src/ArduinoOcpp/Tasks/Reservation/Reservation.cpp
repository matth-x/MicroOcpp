// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Reservation/Reservation.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Debug.h>

#ifndef RESERVATION_FN
#define RESERVATION_FN (AO_FILENAME_PREFIX "reservations.jsn")
#endif

using namespace ArduinoOcpp;

Reservation::Reservation(Model& model, unsigned int slot) : model(model), slot(slot) {
    const size_t KEY_SIZE = 50;
    char key [KEY_SIZE] = {'\0'};

    snprintf(key, KEY_SIZE, "cid_%u", slot);
    connectorId = declareConfiguration<int>(key, -1, RESERVATION_FN, false, false, true, false);

    snprintf(key, KEY_SIZE, "expdt_%u", slot);
    expiryDateRaw = declareConfiguration<const char*>(key, "", RESERVATION_FN, false, false, true, false);
    expiryDate.setTime(*expiryDateRaw);
    
    snprintf(key, KEY_SIZE, "idt_%u", slot);
    idTag = declareConfiguration<const char*>(key, "", RESERVATION_FN, false, false, true, false);

    snprintf(key, KEY_SIZE, "rsvid_%u", slot);
    reservationId = declareConfiguration<int>(key, -1, RESERVATION_FN, false, false, true, false);

    snprintf(key, KEY_SIZE, "pidt_%u", slot);
    parentIdTag = declareConfiguration<const char*>(key, "", RESERVATION_FN, false, false, true, false);

    if (!connectorId || !expiryDateRaw || !idTag || !reservationId || !parentIdTag) {
        AO_DBG_ERR("initialization failure");
        (void)0;
    }
}

bool Reservation::isActive() {
    if (*connectorId < 0) {
        //reservation invalidated
        return false;
    }

    auto& t_now = model.getTime().getTimestampNow();
    if (t_now > expiryDate) {
        //reservation expired
        return false;
    }

    return true;
}

bool Reservation::matches(unsigned int connectorId) {
    return (int) connectorId == *this->connectorId;
}

bool Reservation::matches(const char *idTag, const char *parentIdTag) {
    if (idTag == nullptr && parentIdTag == nullptr) {
        return true;
    }

    if (idTag && !strcmp(idTag, *this->idTag)) {
        return true;
    }

    if (parentIdTag && !strcmp(parentIdTag, *this->parentIdTag)) {
        return true;
    }

    return false;
}

int Reservation::getConnectorId() {
    return *connectorId;
}

Timestamp& Reservation::getExpiryDate() {
    return expiryDate;
}

const char *Reservation::getIdTag() {
    return *idTag;
}

int Reservation::getReservationId() {
    return *reservationId;
}

const char *Reservation::getParentIdTag() {
    return parentIdTag->getBuffsize() <= 1 ? (const char *) nullptr : *parentIdTag;
}

void Reservation::update(JsonObject payload) {
    *connectorId = payload["connectorId"] | -1;
    expiryDate.setTime(payload["expiryDate"]);
    *expiryDateRaw = payload["expiryDate"].as<const char*>();
    *idTag = payload["idTag"].as<const char*>();
    *reservationId = payload["reservationId"] | -1;
    *parentIdTag = payload["parentIdTag"] | "";

    configuration_save();
}

void Reservation::clear() {
    *connectorId = -1;
    expiryDate = MIN_TIME;
    *expiryDateRaw = "";
    *idTag = "";
    *reservationId = -1;
    *parentIdTag = "";

    configuration_save();
}
