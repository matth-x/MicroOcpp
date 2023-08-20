// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti WiFiMulti;
#elif defined(ESP32)
#include <WiFi.h>
#else
#error only ESP32 or ESP8266 supported at the moment
#endif

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h> //need for setting TLS credentials

#define STASSID  "YOUR_WIFI_SSID"
#define STAPSK   "YOUR_WIFI_PW"

#define OCPP_HOST  "echo.websocket.events"
#define OCPP_PORT  443
#define OCPP_URL   "wss://echo.websocket.events/"

/*
 * OCPP Security Profile 2: TLS with Basic Authentication
 *
 * Example credentials from the OCPP-JSON document (p. 16)
 */
#define OCPP_AUTH_ID  "AL1000"
#define OCPP_AUTH_KEY "0001020304050607FFFFFFFFFFFFFFFFFFFFFFFF"

const char ENDPOINT_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

WebSocketsClient wsockSecure {};
MicroOcpp::EspWiFi::WSClient osockSecure {&wsockSecure};

void setup() {

    /*
     * Initialize Serial and WiFi
     */ 

    Serial.begin(115200);

    Serial.print(F("[main] Wait for WiFi "));

#if defined(ESP8266)
    WiFiMulti.addAP(STASSID, STAPSK);
    while (WiFiMulti.run() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
#elif defined(ESP32)
    WiFi.begin(STASSID, STAPSK);
    while (!WiFi.isConnected()) {
        delay(1000);
        Serial.print('.');
    }
#endif

    Serial.print(F(" connected\n"));

    /*
     * Set system time (required for Certificate validation)
     */
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //alternatively: settimeofday(&de, &tz);
    Serial.print(F("[main] Wait for NTP time (used for certificate validation) "));
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(1000);
        Serial.print('.');
        now = time(nullptr);
    }
    Serial.printf(" finished. Unix timestamp is %lu\n", now);
    
    /*
     * Connect to OCPP Central System (using OCPP Security Profile 2: TLS with Basic Authentication )
     */
    wsockSecure.beginSslWithCA(OCPP_HOST,
                    OCPP_PORT,
                    OCPP_URL,
                    ENDPOINT_CA_CERT, "ocpp1.6");
    wsockSecure.setReconnectInterval(5000);
    wsockSecure.enableHeartbeat(15000, 3000, 2);
    wsockSecure.setAuthorization(OCPP_AUTH_ID, OCPP_AUTH_KEY); // => Authorization: Basic QUwxMDAwOgABAgMEBQYH////////////////
    
    mocpp_initialize(osockSecure, ChargerCredentials("My Charging Station", "My company name"));

    /*
     * ... see MicroOcpp.h for how to integrate the EVSE hardware.
     *
     * This example only showcases the TLS connection. For examples about the HW integration,
     * please see the other examples
     */
}

void loop() {

    /*
     * Execute all charge point routines and handle WebSocket
     */
    mocpp_loop();

    //... see MicroOcpp.h and the other examples for how to integrate the EVSE hardware.
}
