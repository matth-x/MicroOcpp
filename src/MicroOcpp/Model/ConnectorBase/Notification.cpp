// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/ConnectorBase/Notification.h>

namespace MicroOcpp {

OCPP_TxNotification convertTxNotification(TxNotification txn) {
    auto res = OCPP_TxNotification::AuthorizationRejected;
    
    switch (txn) {
        case TxNotification::AuthorizationRejected:
            res = OCPP_TxNotification::AuthorizationRejected;
            break;
        case TxNotification::AuthorizationTimeout:
            res = OCPP_TxNotification::AuthorizationTimeout;
            break;
        case TxNotification::Authorized:
            res = OCPP_TxNotification::Authorized;
            break;
        case TxNotification::ConnectionTimeout:
            res = OCPP_TxNotification::ConnectionTimeout;
            break;
        case TxNotification::DeAuthorized:
            res = OCPP_TxNotification::DeAuthorized;
            break;
        case TxNotification::RemoteStart:
            res = OCPP_TxNotification::RemoteStart;
            break;
        case TxNotification::RemoteStop:
            res = OCPP_TxNotification::RemoteStop;
            break;
        case TxNotification::ReservationConflict:
            res = OCPP_TxNotification::ReservationConflict;
            break;
        case TxNotification::StartTx:
            res = OCPP_TxNotification::StartTx;
            break;
        case TxNotification::StopTx:
            res = OCPP_TxNotification::StopTx;
            break;
    }

    return res;
}

} //end namespace MicroOcpp
