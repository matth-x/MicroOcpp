// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2020
// MIT License

#ifndef CONFIGURATION_H
#define CONFIGUATION_H

#include <ArduinoJson.h>

bool configuration_init();
bool configuration_save();

bool setConfiguration_Int(const char *key, int value);
bool setConfiguration_Float(const char *key, float value);
bool setConfiguration_string(const char *key, const char *value);
bool setConfiguration_string(const char *key, const char *value, size_t buffsize);

//TODO change this to declareConfiguration! Only key/value pairs can be saved that have been declared before. Also set rebootRequired and readOnly here
bool defaultConfiguration_Int(const char *key, int value);
bool defaultConfiguration_Float(const char *key, float value);
bool defaultConfiguration_string(const char *key, const char *value);
bool defaultConfiguration_string(const char *key, const char *value, size_t buffsize);

bool getConfiguration_Int(const char *key, int *value);
bool getConfiguration_Float(const char *key, float *value);
bool getConfigurationBuffsize_string(const char *key, size_t *buffsize);
bool getConfigurationBuffer_string(const char *key, char **value);
bool getConfiguration_string(const char *key, char *value);

bool configurationContains_Int(const char *key);
bool configurationContains_Float(const char *key);
bool configurationContains_string(const char *key);

bool checkReadOnly(const char *key);
bool checkRebootRequired(const char *key);

void addChangeConfigurationObserver(const char *key, void action()); //does NOT react on defaultConfiguration_Type(...)

DynamicJsonDocument *serializeConfiguration(const char *key);
DynamicJsonDocument *serializeConfiguration();

#endif
