// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppServer.h>

#include <Variants.h>

#ifndef AO_CUSTOM_WS

using namespace ArduinoOcpp::EspWiFi;

OcppServer::OcppServer() {
    wsockServer.begin();
    wsockServer.onEvent([this](WsClient num, WStype_t type, uint8_t * payload, size_t length) {
        this->wsockEvent(num, type, payload, length);
    });

    instance = this;
}

void OcppServer::wsockEvent(WsClient num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            {
                if (DEBUG_OUT) Serial.print(F("[OcppServer] WsClient disconnected! num = "));
                if (DEBUG_OUT) Serial.println(num);                
            }
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = wsockServer.remoteIP(num);
                if (DEBUG_OUT) {
                    Serial.print(F("[OcppServer] WsClient connected! num = "));
                    Serial.print(num);
                    Serial.print(F(", Connected from IP = "));
                    ip.printTo(Serial);
                    Serial.print(F(", with payload "));
                    Serial.println((const char*) payload);
                }

                bool found = false;
				for (auto route = receiveTXTrouting.begin(); route != receiveTXTrouting.end(); ++route) {
                    if (ip == (*route).ip_addr) {
                        if (DEBUG_OUT) Serial.print(F("[OcppServer] IPAddress matches existing route!\n"));
                        (*route).num = num;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    Serial.print(F("[OcppServer] Unknown IP address! Please see addReceiveTXTcallback(IPAddress ...)\n"));
                }
            }
            break;
        case WStype_TEXT:
            {
                if (DEBUG_OUT || TRAFFIC_OUT) {
                    Serial.print(F("[OcppServer] Get TXT from client: "));
                    Serial.print(num);
                    Serial.print(F(", TXT = "));
                    Serial.println((const char*) payload);
                }

                bool found = false;
                for (auto route = receiveTXTrouting.begin(); route != receiveTXTrouting.end(); ++route) {
                    if (num == (*route).num) {
                        if (DEBUG_OUT) Serial.print(F("\n"));
                        if (!((*route).processTXT((const char*) payload, length))) {
                            Serial.print(F("[OcppServer] Processing WebSocket input event failed!\n"));
                        }
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    Serial.print(F("[OcppServer] Received msg from unknown client!\n"));
                }
            }
            break;
        case WStype_PING:
            // pong will be send automatically
            Serial.printf("[OcppServer] get ping, client = %u\n", num);
            break;
        case WStype_PONG:
            // answer to a ping we send
            Serial.printf("[OcppServer] get pong, client = %u\n", num);
            break;
        case WStype_FRAGMENT_TEXT_START: //fragments are not supported
        case WStype_BIN:
        default:
            Serial.print(F("[OcppServer] Unsupported WebSocket event type, client = "));
            Serial.println(num);
            break;
    }
}

OcppServer *OcppServer::instance = NULL;

OcppServer *OcppServer::getInstance() {
    if (!instance){
        instance = new OcppServer();
    }
    return instance;
}

void OcppServer::loop() {
    wsockServer.loop();
}

void OcppServer::setReceiveTXTcallback(IPAddress &ip_addr, ReceiveTXTcallback &callback) {
    /*
     * Does route identified by ip_addr already exist?
     */
    std::vector<ReceiveTXTroute>::iterator result = std::find_if(receiveTXTrouting.begin(), receiveTXTrouting.end(),
        [ip_addr](const ReceiveTXTroute &elem) {
            return elem.ip_addr == ip_addr;
        });
    
    if (result != receiveTXTrouting.end()) {
        //found a route. Update callback
        (*result).processTXT = callback;
    } else {
        //no route found. Add new one
        ReceiveTXTroute route {};
        route.ip_addr = ip_addr;
        route.processTXT = callback;
        receiveTXTrouting.push_back(route);
    }
}

void OcppServer::removeReceiveTXTcallback(IPAddress &ip_addr) {
    receiveTXTrouting.erase(std::remove_if(receiveTXTrouting.begin(), receiveTXTrouting.end(), 
        [ip_addr](const ReceiveTXTroute &elem) {
            return elem.ip_addr == ip_addr;
        }), receiveTXTrouting.end());
}

bool OcppServer::sendTXT(IPAddress &ip_addr, String &out) {

    WsClient mClient;

    std::vector<ReceiveTXTroute>::iterator result = std::find_if(receiveTXTrouting.begin(), receiveTXTrouting.end(),
        [ip_addr](const ReceiveTXTroute &elem) {
            return elem.ip_addr == ip_addr;
        });
        
    if (result != receiveTXTrouting.end()) {
        mClient = (*result).num;
    } else {
        Serial.print(F("[OcppServer] Tried to send TXT for unregistered IP address! Abort\n"));
        return false;
    }

    return wsockServer.sendTXT(mClient, out);
}

#endif //ndef AO_CUSTOM_WS
