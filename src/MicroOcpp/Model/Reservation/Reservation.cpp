// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Reservation/Reservation.h>

#include <stdio.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

Reservation::Reservation(Context& context, unsigned int slot) : MemoryManaged("v16.Reservation.Reservation"), context(context), slot(slot) {

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

bool Reservation::setup() {

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    int ret;

    ret = snprintf(connectorIdKey, sizeof(connectorIdKey), MO_RESERVATION_CID_KEY "%u", slot);
    if (ret < 0 || (size_t)ret >= sizeof(connectorIdKey)) {
        return false;
    }
    connectorIdInt = configService->declareConfiguration<int>(connectorIdKey, -1, RESERVATION_FN, Mutability::None);

    ret = snprintf(expiryDateRawKey, sizeof(expiryDateRawKey), MO_RESERVATION_EXPDATE_KEY "%u", slot);
    if (ret < 0 || (size_t)ret >= sizeof(expiryDateRawKey)) {
        return false;
    }
    expiryDateRawString = configService->declareConfiguration<const char*>(expiryDateRawKey, "", RESERVATION_FN, Mutability::None);

    ret = snprintf(idTagKey, sizeof(idTagKey), MO_RESERVATION_IDTAG_KEY "%u", slot);
    if (ret < 0 || (size_t)ret >= sizeof(idTagKey)) {
        return false;
    }
    idTagString = configService->declareConfiguration<const char*>(idTagKey, "", RESERVATION_FN, Mutability::None);

    ret = snprintf(reservationIdKey, sizeof(reservationIdKey), MO_RESERVATION_RESID_KEY "%u", slot);
    if (ret < 0 || (size_t)ret >= sizeof(reservationIdKey)) {
        return false;
    }
    reservationIdInt = configService->declareConfiguration<int>(reservationIdKey, -1, RESERVATION_FN, Mutability::None);

    ret = snprintf(parentIdTagKey, sizeof(parentIdTagKey), MO_RESERVATION_PARENTID_KEY "%u", slot);
    if (ret < 0 || (size_t)ret >= sizeof(parentIdTagKey)) {
        return false;
    }
    parentIdTagString = configService->declareConfiguration<const char*>(parentIdTagKey, "", RESERVATION_FN, Mutability::None);

    if (!connectorIdInt || !expiryDateRawString || !idTagString || !reservationIdInt || !parentIdTagString) {
        MO_DBG_ERR("initialization failure");
        return false;
    }

    reservations = configService->findContainerOfConfiguration(connectorIdInt);
    if (!reservations) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    return true;
}

bool Reservation::isActive() {
    if (connectorIdInt->getInt() < 0) {
        //reservation invalidated
        return false;
    }

    int32_t dt;
    if (!context.getClock().delta(context.getClock().now(), getExpiryDate(), dt)) {
        //expiryDate undefined
        return false;
    }

    if (dt >= 0) {
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
    if (!expiryDate.isDefined() && *expiryDateRawString->getString()) {
        if (!context.getClock().parseString(expiryDateRawString->getString(), expiryDate)) {
            MO_DBG_ERR("internal error");
            expiryDate = Timestamp();
        }
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

bool Reservation::update(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag) {
    bool success = true;

    reservationIdInt->setInt(reservationId);
    connectorIdInt->setInt((int) connectorId);
    this->expiryDate = expiryDate;
    char expiryDateStr [MO_INTERNALTIME_SIZE];
    if (context.getClock().toInternalString(this->expiryDate, expiryDateStr, sizeof(expiryDateStr))) {
        success &= expiryDateRawString->setString(expiryDateStr);
    } else {
        success = false;
    }
    success &= idTagString->setString(idTag);
    success &= parentIdTagString->setString(parentIdTag);

    success &= reservations->commit();

    return success; //no roll-back mechanism implemented yet
}

bool Reservation::clear() {
    bool success = true;

    connectorIdInt->setInt(-1);
    expiryDate = Timestamp();
    success &= expiryDateRawString->setString("");
    success &= idTagString->setString("");
    reservationIdInt->setInt(-1);
    success &= parentIdTagString->setString("");

    success &= reservations->commit();

    return success; //no roll-back mechanism implemented yet
}

#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
