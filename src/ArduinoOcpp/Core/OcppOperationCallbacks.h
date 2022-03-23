// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPP_OPERATION_CALLBACKS
#define OCPP_OPERATION_CALLBACKS

#include <ArduinoJson.h>
#include <functional>

#include <ArduinoOcpp/Core/OcppOperationTimeout.h>

namespace ArduinoOcpp {

using OnReceiveConfListener = std::function<void(JsonObject payload)>;
using OnReceiveReqListener = std::function<void(JsonObject payload)>;
using OnSendConfListener = std::function<void(JsonObject payload)>;
//using OnTimeoutListener = std::function<void()>; //in OcppOperationTimeout. Workaround for circle include. Fix by extracting type definitions to new source file
using OnReceiveErrorListener = std::function<void(const char *code, const char *description, JsonObject details)>; //will be called if OCPP communication partner returns error code
//using OnAbortListener = std::function<void()>; //will be called whenever the engine will stop trying to execute the operation normallythere is a timeout or error (onAbort = onTimeout || onReceiveError)


} //end namespace ArduinoOcpp
#endif
