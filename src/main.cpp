#include <Wire.h>
#include <Arduino.h>
#include "FS.h"
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

bool WIFI_CONNECT_FLAG = false;
#define LED_BUILTIN 2

WebSocketsServer webSocket(81);
WebServer server(80);

#define SDA_PIN 21
#define SCL_PIN 22
#define ADC1_ADDRESS 0x68  // Voltage ADC (channels 1,3,4)
#define ADC2_ADDRESS 0x6A  // Current ADC (channels 1,2)

struct InputChannel {
    uint8_t config;
    uint8_t address;  // Added I2C address for each channel
    String name;
};

InputChannel inputs[5] = {
    {0b10001000, ADC1_ADDRESS, "voltage_1"},  // CH1 on ADC1
    {0b11001000, ADC1_ADDRESS, "voltage_3"},  // CH3 on ADC1
    {0b11011000, ADC1_ADDRESS, "voltage_4"},  // CH4 on ADC1
    {0b10001000, ADC2_ADDRESS, "current_1"},  // CH1 on ADC2
    {0b10101000, ADC2_ADDRESS, "current_2"}   // CH2 on ADC2
};

int16_t readADC(uint8_t config, uint8_t address) {
    Wire.beginTransmission(address);
    Wire.write(config);
    Wire.endTransmission();
    delay(100);

    Wire.requestFrom(address, 2);
    if (Wire.available() == 2) {
        return (int16_t)((Wire.read() << 8) | Wire.read());
    }
    Serial.println("Error: No data received from address 0x" + String(address, HEX));
    return 0;
}

void listFiles() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.printf("File: %s Size: %u\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

void serveFile(const char* path, const char* contentType) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.printf("Error: %s not found\n", path);
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, contentType);
    file.close();
}

void setup() {
    Serial.begin(115200);    
    WiFiManager wifiManager; 
    Serial.println("Booting ESP32...");

    Wire.begin(SDA_PIN, SCL_PIN);
    Serial.println("Initializing MCP3426 ADCs...");

    if (!wifiManager.autoConnect("ESP32-Setup")) {
        Serial.println("Failed to connect. Rebooting...");
        ESP.restart();
    }

    Serial.println("Connected! IP Address: " + WiFi.localIP().toString());
    pinMode(LED_BUILTIN, OUTPUT);
    WIFI_CONNECT_FLAG = true;

    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");
    listFiles();

    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                Serial.printf("WebSocket[%u] connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
                break;
            case WStype_DISCONNECTED:
                Serial.printf("WebSocket[%u] disconnected\n", num);
                break;
            case WStype_TEXT:
                Serial.printf("WebSocket[%u] received text: %s\n", num, payload);
                break;
        }
    });

    server.on("/", HTTP_GET, []() { serveFile("/index.html", "text/html"); });
    server.on("/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
    server.on("/script.js", HTTP_GET, []() { serveFile("/script.js", "application/javascript"); });
    server.on("/logo.jpg", HTTP_GET, []() { serveFile("/logo.jpg", "image/jpeg"); });
    
    server.begin();
    Serial.println("HTTP server started");
    Serial.println("ESP32 is hosting at: http://" + WiFi.localIP().toString());
}

void loop() {
    if (WIFI_CONNECT_FLAG) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
    }

    webSocket.loop();
    server.handleClient();

    if (webSocket.connectedClients() > 0) {
        String jsonResponse = "{";
        for (int i = 0; i < 5; i++) {
            int16_t rawData = readADC(inputs[i].config, inputs[i].address);
            float voltage = rawData * (2.048 / 32768.0);
            jsonResponse += "\"" + inputs[i].name + "\": " + String(voltage, 3);
            if (i < 4) jsonResponse += ", ";
        }
        jsonResponse += "}";

        Serial.println("Broadcasting: " + jsonResponse);
        webSocket.broadcastTXT(jsonResponse);
    }

    delay(1000);
}

