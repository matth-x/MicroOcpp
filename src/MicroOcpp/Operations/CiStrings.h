// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

/*
 * A collection of the fixed-length string types in the OCPP specification
 */

#ifndef CI_STRINGS_H
#define CI_STRINGS_H

#define CiString20TypeLen 20
#define CiString25TypeLen 25
#define CiString50TypeLen 50
#define CiString255TypeLen 255
#define CiString500TypeLen 500

//specified by OCPP
#define IDTAG_LEN_MAX CiString20TypeLen
#define CONF_KEYLEN_MAX CiString50TypeLen

//not specified by OCPP
#define REASON_LEN_MAX 15

#endif
