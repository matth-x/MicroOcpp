// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_NOTIFICATION_H
#define MO_NOTIFICATION_H

#ifdef __cplusplus

namespace MicroOcpp {

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

} //end namespace MicroOcpp

extern "C" {
#endif //__cplusplus

enum OCPP_TxNotification {
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

#ifdef __cplusplus
} //end extern "C"

namespace MicroOcpp {

OCPP_TxNotification convertTxNotification(TxNotification txn);

} //end namespace MicroOcpp

#endif //__cplusplus

#endif
