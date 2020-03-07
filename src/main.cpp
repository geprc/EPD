#include <FS.h>
#include <GxEPD.h>
#include <SPIFFS.h>
#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <GxGDEP015OC1/GxGDEP015OC1.h>
#include "qrcode.h"
#include <ArduinoOTA.h>

bool is_use_ap = false;
const char* ssid = "TP-LINK_4135";
const char* password = "ww112233..";

GxIO_Class io(SPI, SS, 17, -1);
GxEPD_Class display(io, -1, -1);
QRcode qrcode(&display);
AsyncWebServer server(80);
WebSocketsServer webSocket(81);

void webSocketEvent(uint8_t, WStype_t, uint8_t *, size_t);

void setup() {
    Serial.begin(115200);
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS initialisation failed!");
        while (1) yield();
    }
    display.init();
    display.setCursor(0, 9);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    Serial.println("Connecting to wifi");
    display.println("Connecting to wifi ......");
    display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
    int i = 0;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        i++;
        Serial.println(i);
        delay(500);
        Serial.print(".");
        if (i > 10) {
            is_use_ap = true;
            break;
        }
    }
    if (!is_use_ap) {
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        qrcode.create("http://" + WiFi.localIP().toString());
        display.updateWindow(0, 0, 200, 200, false);
    } else {
        while (!WiFi.softAP(ssid, password)) {
            Serial.println("set up my wifi");
            delay(200);
        }
        Serial.println("ok");
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(0, 9);
        display.println("Try to connect.");
        Serial.println("Try to connect.");
        display.println("WiFi: " + String(ssid));
        Serial.println("WiFi: " + String(ssid));
        display.println("PassWord:" + String(password));
        Serial.println("PassWord:" + String(password));
        display.print("host: ");
        Serial.print("host");
        display.println(WiFi.softAPIP());
        Serial.println(WiFi.softAPIP());
        display.updateWindow(0, 0, 200, 200, false);
        qrcode.create(WiFi.softAPIP().toString());
    }
    // ws.onEvent(onEvent);
    // server.addHandler(&ws);
    webSocket.onEvent(webSocketEvent);
    webSocket.begin();
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/www/index.html");
        request->send(response);
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/avatar2.jpg", "image/jpeg");
    });
    server.serveStatic("/", SPIFFS, "/www/");
    server.begin();
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else  // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount
            // SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });
    ArduinoOTA.begin();
}

void loop() {
    webSocket.loop();
    ArduinoOTA.handle();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:{
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            break;
        }
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);
            break;
        case WStype_BIN:{
            Serial.printf("[%u] get binary length: %u\n", num, length);
            display.fillScreen(GxEPD_WHITE);
            display.drawBitmap(payload, 0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, GxEPD_BLACK);
            display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
            break;
        }
        case WStype_PING:
        case WStype_PONG:
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}
