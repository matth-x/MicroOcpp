// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/ChargeControl/Notification.h>

namespace ArduinoOcpp {

AO_TxNotification convertTxNotification(TxNotification txn) {
    auto res = AO_TxNotification::AuthorizationRejected;
    
    switch (txn) {
        case TxNotification::AuthorizationRejected:
            res = AO_TxNotification::AuthorizationRejected;
            break;
        case TxNotification::AuthorizationTimeout:
            res = AO_TxNotification::AuthorizationTimeout;
            break;
        case TxNotification::Authorized:
            res = AO_TxNotification::Authorized;
            break;
        case TxNotification::ConnectionTimeout:
            res = AO_TxNotification::ConnectionTimeout;
            break;
        case TxNotification::DeAuthorized:
            res = AO_TxNotification::DeAuthorized;
            break;
        case TxNotification::RemoteStart:
            res = AO_TxNotification::RemoteStart;
            break;
        case TxNotification::RemoteStop:
            res = AO_TxNotification::RemoteStop;
            break;
        case TxNotification::ReservationConflict:
            res = AO_TxNotification::ReservationConflict;
            break;
        case TxNotification::StartTx:
            res = AO_TxNotification::StartTx;
            break;
        case TxNotification::StopTx:
            res = AO_TxNotification::StopTx;
            break;
    }

    return res;
}

} //end namespace ArduinoOcpp
