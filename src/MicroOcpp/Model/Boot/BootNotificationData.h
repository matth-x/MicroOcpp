// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_BOOTNOTIFICATIONDATA_H
#define MO_BOOTNOTIFICATIONDATA_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Data to be sent in BootNotification.
 *
 * Usage:
 * ```
 * // init
 * MO_BootNotificationData bnData;
 * mo_BootNotificationData_init(&bnData);
 *
 * // set payload
 * bnData.chargePointVendor = "My Company Ltd.";
 * bnData.chargePointModel = "Demo Charger";
 *
 * // pass data to MO
 * mo_setBootNotificationData2(bnData); //copies data, i.e. bnData can be invalidated now
 */

typedef struct {
    const char *chargePointModel;
    const char *chargePointVendor;
    const char *firmwareVersion;
    const char *chargePointSerialNumber;
    const char *meterSerialNumber; //OCPP 1.6 only
    const char *meterType; //OCPP 1.6 only
    const char *chargeBoxSerialNumber; //OCPP 1.6 only
    const char *iccid;
    const char *imsi;
} MO_BootNotificationData;

void mo_BootNotificationData_init(MO_BootNotificationData *bnData);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
