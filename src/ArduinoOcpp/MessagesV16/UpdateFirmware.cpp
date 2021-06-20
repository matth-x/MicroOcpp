// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/UpdateFirmware.h>
#include <ArduinoOcpp/Core/OcppTime.h>

using ArduinoOcpp::Ocpp16::UpdateFirmware;

UpdateFirmware::UpdateFirmware() {

}

void UpdateFirmware::processReq(JsonObject payload) {
    /*
    * Process the application data here. Note: you have to implement the FW update procedure in your client code. You have to set
    * a onSendConfListener in which you initiate a FW update (e.g. calling ESPhttpUpdate.update(...) )
    */

   //check location URL. Maybe introduce Same-Origin-Policy?
   if (!payload["location"]) {
       formatError = true;
   }

   //check the integrity of retrieveDate
   const char *retrieveDate = payload["retrieveDate"] | "Invalid";

   OcppTimestamp parser = OcppTimestamp();

   if (!parser.setTime(retrieveDate)) {
       formatError = true;
   }
}

DynamicJsonDocument* UpdateFirmware::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(0);
    JsonObject payload = doc->to<JsonObject>();
    
    return doc;
}