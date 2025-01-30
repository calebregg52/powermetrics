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
#define I2C_ADDRESS 0x68 // MCP3426 I2C address

int16_t readADC()
{
    Wire.requestFrom(I2C_ADDRESS, 2);
    if (Wire.available() == 2)
    {
        uint8_t highByte = Wire.read();
        uint8_t lowByte = Wire.read();
        return (int16_t)((highByte << 8) | lowByte);
    }
    Serial.println("Error: No data received!");
    return 0;
}

void listFiles()
{
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        Serial.print("File: ");
        Serial.print(file.name());
        Serial.print(" Size: ");
        Serial.println(file.size());
        file = root.openNextFile();
    }
}

void setup()
{
    Serial.begin(115200);    // begin serial output
    WiFiManager wifiManager; // wifimanager handler
    Serial.println("Booting ESP32...");

    Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C

    Serial.println("Initializing MCP3426 ADC...");

    // Set Configuration Register for CH1, 16-bit mode, Continuous Conversion, PGA = 1
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(0b10011000); // 16-bit, Continuous, Gain=1, CH1
    Wire.endTransmission();

    // Start the captive portal
    if (!wifiManager.autoConnect("ESP32-Setup"))
    {
        Serial.println("Failed to connect. Rebooting...");
        ESP.restart();
    }

    // Connected to Wi-Fi
    Serial.println("Connected! IP Address: " + WiFi.localIP().toString());
    pinMode(LED_BUILTIN, OUTPUT);
    WIFI_CONNECT_FLAG = true;

    if (SPIFFS.begin(true))
    {

        Serial.println("SPIFFS mounted successfully");
        listFiles(); // List all files in SPIFFS to verify
    }
    else
    {
        Serial.println("Failed to mount SPIFFS");
        return;
    }

    webSocket.begin();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
                      {
        switch (type) {
            case WStype_CONNECTED: {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("WebSocket[%u] connected from %s\n", num, ip.toString().c_str());
                break;
            }
            case WStype_DISCONNECTED:
                Serial.printf("WebSocket[%u] disconnected\n", num);
                break;
            case WStype_TEXT:
                Serial.printf("WebSocket[%u] received text: %s\n", num, payload);
                break;
        } });

    server.on("/", HTTP_GET, []()
              {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            Serial.println("Error: index.html not found");
            server.send(404, "text/plain", "File not found");
            return;
        }
        server.streamFile(file, "text/html");
        file.close(); });

    server.on("/style.css", HTTP_GET, []()
              {
    File file = SPIFFS.open("/style.css", "r");
    if (!file) {
        Serial.println("Error: style.css not found");
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/css");
    file.close(); });

    server.on("/script.js", HTTP_GET, []()
              {
    File file = SPIFFS.open("/script.js", "r");
    if (!file) {
        Serial.println("Error: script.js not found");
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "application/javascript");
    file.close(); });

    server.on("/logo.jpg", HTTP_GET, []()
              {
    File file = SPIFFS.open("/logo.jpg", "r");
    if (!file) {
        Serial.println("Error: logo.jpg not found");
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "image/jpeg");
    file.close(); });

    server.begin();
    Serial.println("HTTP server started");
    Serial.println("ESP32 is hosting at: http://" + WiFi.localIP().toString());
}

void loop()
{

    if (WIFI_CONNECT_FLAG == true)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }

    webSocket.loop();
    server.handleClient();

    int16_t rawData = readADC();
    float voltage = rawData * (2.048 / 32768.0); // Convert to voltage

    String jsonResponse = "{\"voltage\": " + String(voltage, 3) + "}";
    Serial.println("Broadcasting: " + jsonResponse);
    webSocket.broadcastTXT(jsonResponse);

    delay(1000);
}
