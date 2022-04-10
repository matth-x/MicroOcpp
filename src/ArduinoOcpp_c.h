#ifndef ARDUINOOCPP_C_H
#define ARDUINOOCPP_C_H

#include <stddef.h>

struct AO_OcppSocket;
typedef struct AO_OcppSocket AO_OcppSocket;

typedef void (*OnOcppMessage) (const char *payload, size_t len);
typedef void (*OnOcppAbort)   ();
typedef void (*OnOcppTimeout) ();
typedef void (*OnOcppError)   (const char *code, const char *description, const char *details_json, size_t details_len);

#ifdef __cplusplus
extern "C" {
#endif

void ao_initialize(AO_OcppSocket *osock);

void ao_loop();

void ao_bootNotification(const char *chargePointModel, const char *chargePointVendor, OnOcppMessage onConfirmation);

void ao_onResetRequest(OnOcppMessage onRequest);

#ifdef __cplusplus
}
#endif

#endif
