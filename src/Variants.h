// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef VARIANTS_H
#define VARIANTS_H

#ifndef DEBUG_OUT
#ifdef AO_DEBUG_OUT
#define DEBUG_OUT true   //Print debug messages on Serial. Gives more insights about inner processes
#else
#define DEBUG_OUT false   //Print debug messages on Serial. Gives more insights about inner processes
#endif
#endif

#ifndef TRAFFIC_OUT
#ifdef AO_TRAFFIC_OUT
#define TRAFFIC_OUT true  //Print OCPP communication on Serial. Gives insights about communication with the Central System
#else
#define TRAFFIC_OUT false
#endif
#endif

#ifndef USE_FACADE
#define USE_FACADE true
#endif

#endif
