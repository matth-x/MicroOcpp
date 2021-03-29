// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

/*
 * Deprecated. Switched to file system
 */

#ifndef EEPROMLAYOUT_H
#define EEPROMLAYOUT_H

#define WS_URL_PREFIX_MAXLENGTH 100
#define MEM_INIT_INDICATOR 2604053329UL // just a random "magic number"

typedef struct {
  float energyActiveImportRegister;
  char ws_url_prefix[WS_URL_PREFIX_MAXLENGTH];
  ulong mem_init_indicator; //set this field to MEM_INIT_INDICATOR before write back, so future accesses can determine that the memory is initialized
} EEPROM_Data;

#endif
