// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVATION_H
#define RESERVATION_H

#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

class OcppModel;

class Reservation {
private:
    OcppModel& context;
    const unsigned int slot;

    std::shared_ptr<Configuration<int>> connectorId;
    std::shared_ptr<Configuration<const char *>> expiryDateRaw;
    OcppTimestamp expiryDate;
    std::shared_ptr<Configuration<const char *>> idTag;
    std::shared_ptr<Configuration<int>> reservationId;
    std::shared_ptr<Configuration<const char *>> parentIdTag;

public:
    Reservation(OcppModel& context, unsigned int slot);

    bool isActive(); //if this object contains a valid, unexpired reservation

    bool matches(unsigned int connectorId);
    bool matches(const char *idTag, const char *parentIdTag = nullptr); //idTag == parentIdTag == nullptr -> return True

    int getConnectorId();
    OcppTimestamp& getExpiryDate();
    const char *getIdTag();
    int getReservationId();
    const char *getParentIdTag();

    void update(JsonObject payload);
    void clear();
};

}

#endif
