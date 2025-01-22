#include <Arduino.h>
#include "FS.h"
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

// Definitions
#define LED_BUILTIN 2
#define VOLT_SENSOR_PIN 35 // ADC pin for voltage sensor
#define V_REF 3.7          // ESP32 reference voltage
#define MAX_ADC 4095.0     // 12-bit ADC resolution

bool WIFI_CONNECT_FLAG = false;
WebSocketsServer webSocket(81);
WebServer server(80);

// Function to list files in SPIFFS
void listFiles()
{
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    Serial.printf("File: %s, Size: %d bytes\n", file.name(), file.size());
    file = root.openNextFile();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting ESP32...");

  // Mount SPIFFS
  if (SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mounted successfully");
    listFiles(); // Optional: Debugging to verify files
  }
  else
  {
    Serial.println("Failed to mount SPIFFS");
    return; // Exit if SPIFFS fails
  }

  // Connect to WiFi using WiFiManager
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-Setup"))
  {
    Serial.println("Failed to connect to WiFi. Rebooting...");
    ESP.restart();
  }
  Serial.println("Connected to WiFi. IP Address: " + WiFi.localIP().toString());

  // Start WebSocket server
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

  // Start HTTP server
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

  webSocket.loop();
  server.handleClient();

  int rawADC = analogRead(VOLT_SENSOR_PIN);
  float adcVoltage = rawADC * (V_REF / MAX_ADC);

  String jsonResponse = "{\"voltage\": " + String(adcVoltage, 3) + "}";
  Serial.println("Broadcasting: " + jsonResponse);
  webSocket.broadcastTXT(jsonResponse);

  delay(10);

}
