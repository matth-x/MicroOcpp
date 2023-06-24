// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_NOTIFICATION_H
#define AO_NOTIFICATION_H

namespace ArduinoOcpp {

enum class TxNotification {
    //Authorization events
    Authorized, //success
    AuthorizationRejected, //IdTag not authorized
    AuthorizationTimeout, //authorization failed - offline
    ReservationConflict, //connector reserved for other IdTag

    ConnectionTimeout, //user took to long to plug vehicle after the authorization
    DeAuthorized, //server rejected StartTx
    RemoteStart, //authorized via RemoteStartTransaction
    RemoteStop, //stopped via RemoteStopTransaction

    //Tx lifecycle events
    StartTx,
    StopTx,
};

}

#endif
