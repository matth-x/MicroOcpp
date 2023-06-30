// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/ChargeControl/Notification.h>

namespace ArduinoOcpp {

AOTxNotification_c convertTxNotification(TxNotification txn) {
    auto res = AOTxNotification_c::AuthorizationRejected;
    
    switch (txn) {
        case TxNotification::AuthorizationRejected:
            res = AOTxNotification_c::AuthorizationRejected;
            break;
        case TxNotification::AuthorizationTimeout:
            res = AOTxNotification_c::AuthorizationTimeout;
            break;
        case TxNotification::Authorized:
            res = AOTxNotification_c::Authorized;
            break;
        case TxNotification::ConnectionTimeout:
            res = AOTxNotification_c::ConnectionTimeout;
            break;
        case TxNotification::DeAuthorized:
            res = AOTxNotification_c::DeAuthorized;
            break;
        case TxNotification::RemoteStart:
            res = AOTxNotification_c::RemoteStart;
            break;
        case TxNotification::RemoteStop:
            res = AOTxNotification_c::RemoteStop;
            break;
        case TxNotification::ReservationConflict:
            res = AOTxNotification_c::ReservationConflict;
            break;
        case TxNotification::StartTx:
            res = AOTxNotification_c::StartTx;
            break;
        case TxNotification::StopTx:
            res = AOTxNotification_c::StopTx;
            break;
    }

    return res;
}

} //end namespace ArduinoOcpp
