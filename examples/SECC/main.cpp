// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License
//
//
// IMPORTANT: Please add https://github.com/tzapu/WiFiManager.git to the lib_deps of your project!
//
// This sketch demonstrates a complete OCPP Wi-Fi module of an EVSE. You can flash
// an ESP8266/ESP32 with this program, build it into your EVSE and start charging with
// OCPP-connectivity.
//
// The pin mapping and additional comments can be found in this file. You probably need
// to adapt a few things before deployment.
//


#include <Arduino.h>
#include <WiFiManager.h> //please add to lib_deps: https://github.com/tzapu/WiFiManager.git

#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/Configuration.h> //load and save settings of WiFi captive portal

/*
 * Pin mapping part I
 * Interface to the SECC (Supply Equipment Communication Controller, e.g. the SAE J1772 module)
 */
#define AMPERAGE_PIN 4 //modulated as PWM
#if defined(ESP32)
#define PWM_CHANNEL 0 //PWM channel (only for ESP32)
#endif

#define EV_PLUG_PIN 14 // Input pin | Read if an EV is connected to the EVSE
#if defined(ESP32)
#define EV_PLUGGED HIGH
#define EV_UNPLUGGED LOW
#elif defined(ESP8266)
#define EV_PLUGGED LOW
#define EV_UNPLUGGED HIGH
#endif

#define OCPP_CHARGE_PERMISSION_PIN 5 // Output pin | Signal if OCPP allows / forbids energy flow
#define OCPP_CHARGE_PERMITTED HIGH
#define OCPP_CHARGE_FORBIDDEN LOW

#define EV_CHARGE_PIN 12 // Input pin | Read if EV requests energy (corresponds to SAE J1772 State C)
#if defined(ESP32)
#define EV_CHARGING LOW
#define EV_SUSPENDED HIGH
#elif defined(ESP8266)
#define EV_CHARGING HIGH
#define EV_SUSPENDED LOW
#endif

#define OCPP_AVAILABILITY_PIN 0 // Output pin | Signal if this EVSE is out of order (set by Central System)
#define OCPP_AVAILABLE HIGH
#define OCPP_UNAVAILABLE LOW

#define EVSE_GROUND_FAULT_PIN 13 // Input pin | Read ground fault detector
#define EVSE_GROUND_FAULTED HIGH
#define EVSE_GROUND_CLEAR LOW

/*
 * Pin mapping part II
 * Internal LED of ESP8266/ESP32 + additional ESP8266 development board LED
 */

#define SERVER_CONNECT_LED 16 // Output pin | Signal if connection to OCPP server has succeeded
#define SERVER_CONNECT_ON LOW
#define SERVER_CONNECT_OFF HIGH

#define CHARGE_PERMISSION_LED 2 // Output pin | Signal if OCPP allows / forbids energy flow
#if defined(ESP32)
#define CHARGE_PERMISSION_ON HIGH
#define CHARGE_PERMISSION_OFF LOW
#elif defined(ESP8266)
#define CHARGE_PERMISSION_ON LOW
#define CHARGE_PERMISSION_OFF HIGH
#endif

#if !defined(SECC_NO_DEBUG)
#define PRINT(...) Serial.print(__VA_ARGS__)
#define PRINTF(...) Serial.printf(__VA_ARGS__)
#define PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define PRINT(...)
#define PRINTF(...)
#define PRINTLN(...)
#endif

WebSocketsClient wSock;
ArduinoOcpp::EspWiFi::OcppClientSocket oSock{&wSock};

int evPlugged = EV_UNPLUGGED;

bool booted = false;
ulong scheduleReboot = 0; //0 = no reboot scheduled; otherwise reboot scheduled in X ms
ulong reboot_timestamp = 0; //timestamp of the triggering event; if scheduleReboot=0, the timestamp has no meaning

// ============ CAPTIVE PORTAL
#define CAPTIVE_PORTAL_TIMEOUT  60 //in seconds
#define WIFI_CONNECTION_TIMEOUT 30 //in seconds

struct Ocpp_URL {
    bool isTLS = true;
    String host = String('\0');
    uint16_t port = 443;
    String url = String('\0');
    bool parse(String& url);
};

Ocpp_URL ocppUrlParsed = Ocpp_URL();

bool runWiFiManager();
// ============ END CAPTIVE PORTAL

void setup() {

    /*
     * Initialize peripherals
     */
    Serial.begin(115200);
#if !defined(SECC_NO_DEBUG)
    Serial.setDebugOutput(true);
#endif
    pinMode(EV_PLUG_PIN, INPUT);
    pinMode(EV_CHARGE_PIN, INPUT);
    pinMode(EVSE_GROUND_FAULT_PIN, INPUT);
    pinMode(OCPP_CHARGE_PERMISSION_PIN, OUTPUT);
    digitalWrite(OCPP_CHARGE_PERMISSION_PIN, OCPP_CHARGE_FORBIDDEN);
    pinMode(OCPP_AVAILABILITY_PIN, OUTPUT);
    digitalWrite(OCPP_AVAILABILITY_PIN, OCPP_UNAVAILABLE);
    pinMode(CHARGE_PERMISSION_LED, OUTPUT);
    digitalWrite(CHARGE_PERMISSION_LED, CHARGE_PERMISSION_OFF);

    pinMode(AMPERAGE_PIN, OUTPUT);
#if defined(ESP32)
    pinMode(AMPERAGE_PIN, OUTPUT);
    ledcSetup(PWM_CHANNEL, 1000, 8); //channel=PWM_CHANNEL, freq=1000Hz, range=(2^8)-1
    ledcAttachPin(AMPERAGE_PIN, PWM_CHANNEL); //pin, channel
    ledcWrite(PWM_CHANNEL, 256); //channel, duty cycle (256 is constant +3.3V DC)
#elif defined(ESP8266)
    analogWriteRange(255); //range=(2^8)-1
    analogWriteFreq(1000); //freq=1000Hz
    analogWrite(AMPERAGE_PIN, 256); //256 is constant +3.3V DC
#else
#error Can only run on ESP8266 and ESP32 on the Arduino platform!
#endif

    pinMode(SERVER_CONNECT_LED, OUTPUT);
    digitalWrite(SERVER_CONNECT_LED, SERVER_CONNECT_ON); //signal device reboot
    delay(100);
    digitalWrite(SERVER_CONNECT_LED, SERVER_CONNECT_OFF);

    /*
     * You can use ArduinoOcpp's internal configurations store for credentials other than those which are
     * specified by OCPP. To use it before ArduinoOcpp is initialized, you need to call configuration_init()
     * 
     * This snippet also shows how to integrate a custom filesystem. Just subclass FilesystemAdapter and pass
     * it to the library
     */
    std::shared_ptr<ArduinoOcpp::FilesystemAdapter> filesystem = ArduinoOcpp::makeDefaultFilesystemAdapter(ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail);
    ArduinoOcpp::configuration_init(filesystem);

    /*
     * WiFiManager opens a captive portal, lets the user enter the WiFi credentials and provides a settings
     * page for the OCPP connection
     */
    if (!runWiFiManager()) {
        //couldn't connect
        PRINTLN(F("[main] Couldn't connect to WiFi after multiple attempts"));
        delay(30000);
        ESP.restart();
    }

    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocppUrl = ArduinoOcpp::declareConfiguration<const char *>(
        "ocppUrl", "", CONFIGURATION_FN, false, false, true, true);
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> CA_cert = ArduinoOcpp::declareConfiguration<const char *>(
        "CA_cert", "", CONFIGURATION_FN, false, false, true, true);
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> httpAuthentication = ArduinoOcpp::declareConfiguration<const char *>(
        "httpAuthentication", "", CONFIGURATION_FN, false, false, true, true);
    
    String ocppUrlString = String(*ocppUrl);
    if (!ocppUrlParsed.parse(ocppUrlString)) {
        PRINTLN(F("[main] Invalid OCPP URL. Restart."));
        delay(10000);
        ESP.restart();
    }

    PRINTF("[main] host, port, URL: %s, %hu, %s\n",
                ocppUrlParsed.host.isEmpty() ? "undefined" : ocppUrlParsed.host.c_str(),
                ocppUrlParsed.port,
                ocppUrlParsed.url.isEmpty()  ? "undefined" : ocppUrlParsed.url.c_str());

    /*
     * Initialize ArduinoOcpp framework.
     */
    wSock.setReconnectInterval(5000);
    wSock.enableHeartbeat(15000, 3000, 2);

    if (httpAuthentication->getBuffsize() > 1) {
        wSock.setAuthorization(*httpAuthentication);
    }
    
    if (ocppUrlParsed.isTLS) {
        if (CA_cert->getBuffsize() > 0) {
            //TODOS: ESP8266: limit BearSSL buffsize
            configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //alternatively: settimeofday(&de, &tz);
            PRINT(F("[main] Wait for NTP time (used for certificate validation) "));
            time_t now = time(nullptr);
            while (now < 8 * 3600 * 2) {
                delay(500);
                PRINT('.');
                now = time(nullptr);
            }
            PRINTF(" finished. Unix timestamp is %lu\n", now);

            wSock.beginSslWithCA(ocppUrlParsed.host.c_str(), ocppUrlParsed.port, ocppUrlParsed.url.c_str(), *CA_cert, "ocpp1.6");
        } else {
            wSock.beginSSL(ocppUrlParsed.host.c_str(), ocppUrlParsed.port, ocppUrlParsed.url.c_str(), nullptr, "ocpp1.6");
        }
    } else {
        wSock.begin(ocppUrlParsed.host, ocppUrlParsed.port, ocppUrlParsed.url, "ocpp1.6");
    }

    OCPP_initialize(oSock,
            230.f, //European grid voltage
            ArduinoOcpp::FilesystemOpt::Use_Mount_FormatOnFail);

    /*
     * Integrate OCPP functionality. You can leave out the following part if your EVSE doesn't need it.
     */
    setEnergyMeterInput([]() {
        //read the energy input register of the EVSE here and return the value in Wh
        /*
         * Approximated value. Replace with real reading
         */
        static ulong lastSampled = millis();
        static float energyMeter = 0.f;
        if (getTransactionId() > 0)
            energyMeter += ((float) (millis() - lastSampled)) * 0.003f; //increase by 0.003Wh per ms (~ 10.8kWh per h)
        lastSampled = millis();
        return energyMeter;
    });

    setSmartChargingOutput([](float limit) {
        //set the SAE J1772 Control Pilot value here
        const float voltage = 230.f; // European grid
        const uint nPhases = 1; //one, two or three phase charging
        float amps = limit / (voltage * (float) nPhases);
        if (amps > 51.f)
            amps = 51.f;

        PRINTF("[main] Smart Charging allows maximum charge rate: %iW; convert to Control Pilot amerage: %.2fA\n", (int) limit, amps);

        int pwmVal;
        if (amps < 6.f) {
            pwmVal = 256; // = constant +3.3V DC
        } else {
            pwmVal = (int) (4.26667f * amps);
        }

#if defined(ESP32)
        ledcWrite(PWM_CHANNEL, pwmVal);
#elif defined(ESP8266)
        analogWrite(AMPERAGE_PIN, pwmVal);
#endif
    });

    setEvReadyInput([]() {
        //return true if the EV is in state "Ready for charging" (see https://en.wikipedia.org/wiki/SAE_J1772#Control_Pilot)
        return digitalRead(EV_CHARGE_PIN) == EV_CHARGING;
    });

    addErrorCodeInput([] () {
        //Uncomment if Ground fault pin is used
        //if (digitalRead(EVSE_GROUND_FAULT_PIN) != EVSE_GROUND_CLEAR) {
        //    return "GroundFault";
        //}
        return (const char *) nullptr;
    });

    setOnResetSendConf([] (JsonObject confirmation) {
        if (getTransactionId() >= 0)
            stopTransaction();
        
        PRINTLN(F("[main] Execute reset command"));
        reboot_timestamp = millis();
        scheduleReboot = 5000; //reboot will be executed in loop()
        booted = false;
    });

    //... see ArduinoOcpp.h for more settings

    /*
     * Notify the Central System that this station is ready
     */
    bootNotification("My Charging Station", "My company name", [] (JsonObject response) {
        if (response["status"].as<String>().equals("Accepted")) {
            booted = true;
            digitalWrite(SERVER_CONNECT_LED, SERVER_CONNECT_ON);
        } else {
            //Wait for the connection retry
            reboot_timestamp = millis();
            scheduleReboot = 60000; //wait for 60s until reboot; reboot will be executed in loop()
        }
    });
}

void loop() {

    /*
     * Do all OCPP stuff (process WebSocket input, send recorded meter values to Central System, etc.)
     */
    OCPP_loop();

    //NFC reader integration example
    if (/* RFID chip detected? */ false) {
        const char *idTag = "my-id-tag"; //e.g. idTag = RFID.readIdTag();
        authorize(idTag);
    }

    if (ocppPermitsCharge()) {
        //EVSE is in a charging session and charging is permitted by the OCPP server
        digitalWrite(OCPP_CHARGE_PERMISSION_PIN, OCPP_CHARGE_PERMITTED);
        digitalWrite(CHARGE_PERMISSION_LED, CHARGE_PERMISSION_ON);
    } else {
        //Charging is not allowed due to OCPP rules
        digitalWrite(OCPP_CHARGE_PERMISSION_PIN, OCPP_CHARGE_FORBIDDEN);
        digitalWrite(CHARGE_PERMISSION_LED, CHARGE_PERMISSION_OFF);
    }

    if (scheduleReboot > 0 && millis() - reboot_timestamp >= scheduleReboot) {
        ESP.restart();
    }

    if (!booted) {
        return;
    }

    auto readEvPlugged = digitalRead(EV_PLUG_PIN);
    if (evPlugged == EV_UNPLUGGED && readEvPlugged == EV_PLUGGED //transition from unplugged to plugged
                && getTransactionId() < 0  //no transaction yet
                && isOperative()) {        //EVSE is in operative mode
        startTransaction("my-id-tag");
    } else if (evPlugged == EV_PLUGGED && readEvPlugged == EV_UNPLUGGED //transition from plugged to unplugged
                && getTransactionId() >= 0) { //need to stop transaction
        stopTransaction();
    }
    evPlugged = readEvPlugged;

    //... see ArduinoOcpp.h for more possibilities
}



// ### End of the OCPP integration. The following code integrates the
// ### WiFi-Manager into this example sketch.

bool runWiFiManager() {

    /*
     * Initialize WiFi and start captive portal to set connection credentials
     */ 
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> ocppUrl = ArduinoOcpp::declareConfiguration<const char *>(
        "ocppUrl", "", CONFIGURATION_FN, false, false, true, true);
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> CA_cert = ArduinoOcpp::declareConfiguration<const char *>(
        "CA_cert", "", CONFIGURATION_FN, false, false, true, true);
    std::shared_ptr<ArduinoOcpp::Configuration<const char *>> httpAuthentication = ArduinoOcpp::declareConfiguration<const char *>(
        "httpAuthentication", "", CONFIGURATION_FN, false, false, true, true);

    WiFiManager wifiManager;
    wifiManager.setTitle("EVSE configuration portal");
    wifiManager.setParamsPage(true);
    WiFiManagerParameter ocppUrlParam ("ocppUrl", "OCPP 1.6 Server URL + Charge Box ID", *ocppUrl, 150,"placeholder=\"wss://&lt;domain&gt;:&lt;port&gt;/&lt;path&gt;/&lt;chargeBoxId&gt;\"");
    wifiManager.addParameter(&ocppUrlParam);
    
    WiFiManagerParameter divider("<div><br/></div>");
    wifiManager.addParameter(&divider);

    WiFiManagerParameter caCertParam ("caCert", "CA Certificate", "", 1500,"placeholder=\"Paste here or leave blank\"");
    wifiManager.addParameter(&caCertParam);
    WiFiManagerParameter caCertSavePararm ("<div><label for='caCertSave' style='width: auto;'>Save new CA Certificate</label><input type='checkbox' name='caCertSave' value='1'  style='width: auto; margin-left: 10px; vertical-align: middle;'></div>");
    wifiManager.addParameter(&caCertSavePararm);

    WiFiManagerParameter httpAuthenticationParam ("httpAuthentication", "HTTP-authentication token", "", 150,"placeholder=\"e.g.: aKu0Q4NjMRC9yO3wRo08\"");
    wifiManager.addParameter(&httpAuthenticationParam);
    WiFiManagerParameter httpAuthenticationSavePararm ("<div><label for='httpAuthenticationSave' style='width: auto;'>Save new HTTP token</label><input type='checkbox' name='httpAuthenticationSave' value='1'  style='width: auto; margin-left: 10px; vertical-align: middle;'></div>");
    wifiManager.addParameter(&httpAuthenticationSavePararm);

    wifiManager.setSaveParamsCallback([&ocppUrlParam, ocppUrl, &wifiManager, CA_cert, &caCertParam, httpAuthentication, &httpAuthenticationParam] () {
        String newOcppUrl = String(ocppUrlParam.getValue());
        newOcppUrl.trim();
        Ocpp_URL newOcppUrlParsed = Ocpp_URL();
        if (newOcppUrlParsed.parse(newOcppUrl)) {
            //success
            *ocppUrl = newOcppUrl.c_str();
        }
        
        bool caCertSave = false;
        String caCertChangedParam = String ("caCertSave");
        if(wifiManager.server->hasArg(caCertChangedParam)) {
            String caCertChanged = String(wifiManager.server->arg(caCertChangedParam));
            if (caCertChanged.length() > 0) {
                caCertSave = caCertChanged.charAt(0) == '1' || caCertChanged.charAt(0) == 't' || caCertChanged.charAt(0) == 'o';
            }
        }
        if (caCertSave) {
            *CA_cert = caCertParam.getValue();
        }

        bool httpAuthenticationSave = false;
        String authChangedParam = String ("httpAuthenticationSave");
        if(wifiManager.server->hasArg(authChangedParam)) {
            String authChanged = String(wifiManager.server->arg(authChangedParam));
            if (authChanged.length() > 0) {
                httpAuthenticationSave = authChanged.charAt(0) == '1' || authChanged.charAt(0) == 't' || authChanged.charAt(0) == 'o';
            }
        }
        if (httpAuthenticationSave) {
            *httpAuthentication = httpAuthenticationParam.getValue();
        }
        ArduinoOcpp::configuration_save();
    });

    wifiManager.setDarkMode(true);

    //wifiManager.setConfigPortalTimeout(CAPATITIVE_PORTAL_TIMEOUT / 1000); //if nobody logs in to the portal, continue after timeout 
    wifiManager.setTimeout(CAPTIVE_PORTAL_TIMEOUT); //if nobody logs in to the portal, continue after timeout 
    wifiManager.setConnectTimeout(WIFI_CONNECTION_TIMEOUT);
    //wifiManager.setSaveConnect(true);
    wifiManager.setAPClientCheck(true); // avoid timeout if client connected to softap
    PRINTLN(F("[main] Start capatitive portal"));
    
    if (wifiManager.startConfigPortal("EVSE-Config", "evse1234")) {
        return true;
    } else {
        return wifiManager.autoConnect("EVSE-Config", "evse1234");
    }
}

bool Ocpp_URL::parse(String &ocppUrl) {
    String inputUrl = String(ocppUrl); // wss://host.com:433/path
    inputUrl.trim();

    if (ocppUrl.isEmpty()) {
        return false;
    }
    url = inputUrl;

    inputUrl.toLowerCase();

    if (inputUrl.startsWith("wss://")) {
        isTLS = true;
        port = 443;
    } else if (inputUrl.startsWith("ws://")) {
        isTLS = false;
        port = 80;
    } else {
        return false;
    }
    inputUrl = inputUrl.substring(inputUrl.indexOf("://") + strlen("://")); // host.com:433/path
    if (inputUrl.isEmpty()) {
        return false;
    }

    host = String('\0');
    for (uint i = 0; i < inputUrl.length(); i++) {
        if (inputUrl.charAt(i) == ':') { //case host.com:433/path
            host = inputUrl.substring(0, i);
            uint16_t inputPort = 0;
            for (unsigned int j = i + 1; j < inputUrl.length(); j++) {
                if (isDigit(inputUrl.charAt(j))) {
                    inputPort *= (uint16_t) 10;
                    inputPort += inputUrl.charAt(j) - '0';
                } else {
                    break;
                }
            }
            if (inputPort > 0) {
                port = inputPort;
            }
            break;
        } else if (inputUrl.charAt(i) == '/') { //case host.com/path
            host = inputUrl.substring(0, i);
            break;
        }
    }
    if (host.isEmpty()) {
        host = inputUrl;
    }

    return true;
}
