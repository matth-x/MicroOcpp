// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Reservation/Reservation.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

Reservation::Reservation(Model& model, unsigned int slot) : model(model), slot(slot) {
    
    snprintf(connectorIdKey, sizeof(connectorIdKey), MO_RESERVATION_CID_KEY "%u", slot);
    connectorIdInt = declareConfiguration<int>(connectorIdKey, -1, RESERVATION_FN, false, false, false);

    snprintf(expiryDateRawKey, sizeof(expiryDateRawKey), MO_RESERVATION_EXPDATE_KEY "%u", slot);
    expiryDateRawString = declareConfiguration<const char*>(expiryDateRawKey, "", RESERVATION_FN, false, false, false);
    
    snprintf(idTagKey, sizeof(idTagKey), MO_RESERVATION_IDTAG_KEY "%u", slot);
    idTagString = declareConfiguration<const char*>(idTagKey, "", RESERVATION_FN, false, false, false);

    snprintf(reservationIdKey, sizeof(reservationIdKey), MO_RESERVATION_RESID_KEY "%u", slot);
    reservationIdInt = declareConfiguration<int>(reservationIdKey, -1, RESERVATION_FN, false, false, false);

    snprintf(parentIdTagKey, sizeof(parentIdTagKey), MO_RESERVATION_PARENTID_KEY "%u", slot);
    parentIdTagString = declareConfiguration<const char*>(parentIdTagKey, "", RESERVATION_FN, false, false, false);

    if (!connectorIdInt || !expiryDateRawString || !idTagString || !reservationIdInt || !parentIdTagString) {
        MO_DBG_ERR("initialization failure");
        (void)0;
    }
}

Reservation::~Reservation() {
    if (connectorIdInt->getKey() == connectorIdKey) {
        connectorIdInt->setKey(nullptr);
    }
    if (expiryDateRawString->getKey() == expiryDateRawKey) {
        expiryDateRawString->setKey(nullptr);
    }
    if (idTagString->getKey() == idTagKey) {
        idTagString->setKey(nullptr);
    }
    if (reservationIdInt->getKey() == reservationIdKey) {
        reservationIdInt->setKey(nullptr);
    }
    if (parentIdTagString->getKey() == parentIdTagKey) {
        parentIdTagString->setKey(nullptr);
    }
}

bool Reservation::isActive() {
    if (connectorIdInt->getInt() < 0) {
        //reservation invalidated
        return false;
    }

    if (model.getClock().now() > getExpiryDate()) {
        //reservation expired
        return false;
    }

    return true;
}

bool Reservation::matches(unsigned int connectorId) {
    return (int) connectorId == connectorIdInt->getInt();
}

bool Reservation::matches(const char *idTag, const char *parentIdTag) {
    if (idTag == nullptr && parentIdTag == nullptr) {
        return true;
    }

    if (idTag && !strcmp(idTag, idTagString->getString())) {
        return true;
    }

    if (parentIdTag && !strcmp(parentIdTag, parentIdTagString->getString())) {
        return true;
    }

    return false;
}

int Reservation::getConnectorId() {
    return connectorIdInt->getInt();
}

Timestamp& Reservation::getExpiryDate() {
    if (expiryDate == MIN_TIME && *expiryDateRawString->getString()) {
        expiryDate.setTime(expiryDateRawString->getString());
    }
    return expiryDate;
}

const char *Reservation::getIdTag() {
    return idTagString->getString();
}

int Reservation::getReservationId() {
    return reservationIdInt->getInt();
}

const char *Reservation::getParentIdTag() {
    return parentIdTagString->getString();
}

void Reservation::update(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag) {
    reservationIdInt->setInt(reservationId);
    connectorIdInt->setInt((int) connectorId);
    this->expiryDate = expiryDate;
    char expiryDate_cstr [JSONDATE_LENGTH + 1];
    if (this->expiryDate.toJsonString(expiryDate_cstr, JSONDATE_LENGTH + 1)) {
        expiryDateRawString->setString(expiryDate_cstr);
    }
    idTagString->setString(idTag);
    parentIdTagString->setString(parentIdTag);

    configuration_save();
}

void Reservation::clear() {
    connectorIdInt->setInt(-1);
    expiryDate = MIN_TIME;
    expiryDateRawString->setString("");
    idTagString->setString("");
    reservationIdInt->setInt(-1);
    parentIdTagString->setString("");

    configuration_save();
}
