#include <Wire.h>
#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <esp_wpa2.h>

// Forward declaration
String getTime();

// Constants
const int LED_PIN = 2;
const uint32_t LED_BLINK_INTERVAL_MS = 500;
const uint32_t ADC_READ_INTERVAL_MS = 500;
const uint32_t ADC_CONVERSION_DELAY_MS = 70;
const uint8_t ADC1_ADDRESS = 0x68;  // Voltage ADC (channels 1, 3, 4)
const uint8_t ADC2_ADDRESS = 0x6A;  // Current ADC (channels 1, 2)
const uint8_t SDA_PIN = 21;
const uint8_t SCL_PIN = 22;
const uint32_t WDT_TIMEOUT_S = 10;
const char* LOG_FILE = "/adc_log.csv";
const char* WIFI_CONFIG_FILE = "/wifi_config.json";

// ADC Channel Configuration
struct AdcChannel {
    uint8_t config;
    uint8_t address;
    const char* name;
    int16_t value;
    bool enabled;
    float multiplier;  // Slope from the linear equation
    float offset;     // Intercept from the linear equation
};

AdcChannel adcChannels[] = {
    {0b10001000, ADC1_ADDRESS, "voltage_1", 0, true, 11.76, -0.015},  // y = 11.76x - 0.015
    {0b11001000, ADC1_ADDRESS, "voltage_3", 0, true, 11.74, -0.020},  // y = 11.74x - 0.020
    {0b11011000, ADC1_ADDRESS, "voltage_4", 0, true, 11.73, -0.022},  // y = 11.73x - 0.022
    {0b10001000, ADC2_ADDRESS, "current_1", 0, true, 1.0, 0.0},
    {0b10101000, ADC2_ADDRESS, "current_2", 0, true, 1.0, 0.0}
};
// Global Objects
WebSocketsServer webSocket(81);
WebServer server(80);
bool wifiConnected = false;

// Structure to hold WiFi settings
struct WifiConfig {
    bool useEnterprise;
    String enterpriseSsid;
    String enterpriseUsername;
    String enterprisePassword;
};

// Read WiFi settings from SPIFFS
WifiConfig loadWifiConfig() {
    WifiConfig config = {false, "", "", ""};
    File file = SPIFFS.open(WIFI_CONFIG_FILE, "r");
    if (!file) {
        Serial.println("Failed to open WiFi config file for reading");
        return config;
    }

    String content = file.readString();
    file.close();

    int useEnterpriseIdx = content.indexOf("\"useEnterprise\":");
    int ssidIdx = content.indexOf("\"enterpriseSsid\":");
    int usernameIdx = content.indexOf("\"enterpriseUsername\":");
    int passwordIdx = content.indexOf("\"enterprisePassword\":");

    if (useEnterpriseIdx != -1) {
        int valueStart = content.indexOf(":", useEnterpriseIdx) + 1;
        int valueEnd = content.indexOf(",", valueStart);
        if (valueEnd == -1) valueEnd = content.indexOf("}", valueStart);
        String value = content.substring(valueStart, valueEnd);
        config.useEnterprise = (value.indexOf("true") != -1);
    }

    if (ssidIdx != -1) {
        int valueStart = content.indexOf("\"", ssidIdx + 15) + 1;
        int valueEnd = content.indexOf("\"", valueStart);
        config.enterpriseSsid = content.substring(valueStart, valueEnd);
    }

    if (usernameIdx != -1) {
        int valueStart = content.indexOf("\"", usernameIdx + 19) + 1;
        int valueEnd = content.indexOf("\"", valueStart);
        config.enterpriseUsername = content.substring(valueStart, valueEnd);
    }

    if (passwordIdx != -1) {
        int valueStart = content.indexOf("\"", passwordIdx + 19) + 1;
        int valueEnd = content.indexOf("\"", valueStart);
        config.enterprisePassword = content.substring(valueStart, valueEnd);
    }

    return config;
}

// Save WiFi settings to SPIFFS
void saveWifiConfig(const WifiConfig& config) {
    File file = SPIFFS.open(WIFI_CONFIG_FILE, "w");
    if (!file) {
        Serial.println("Failed to open WiFi config file for writing");
        return;
    }

    String json = "{";
    json += "\"useEnterprise\":" + String(config.useEnterprise ? "true" : "false") + ",";
    json += "\"enterpriseSsid\":\"" + config.enterpriseSsid + "\",";
    json += "\"enterpriseUsername\":\"" + config.enterpriseUsername + "\",";
    json += "\"enterprisePassword\":\"" + config.enterprisePassword + "\"";
    json += "}";

    file.print(json);
    file.close();
    Serial.println("WiFi config saved");
}

// Read a single ADC channel
int16_t readSingleChannel(const AdcChannel& channel) {
    Wire.beginTransmission(channel.address);
    Wire.write(channel.config);
    Wire.endTransmission();
    delay(ADC_CONVERSION_DELAY_MS);
    Wire.requestFrom((uint8_t)channel.address, (uint8_t)2);
    if (Wire.available() == 2) {
        return (Wire.read() << 8) | Wire.read();
    }
    Serial.printf("Error: No data from 0x%02X\n", channel.address);
    return 0;
}

// Update all ADC values
void updateAdcValues() {
    for (auto& channel : adcChannels) {
        if (channel.enabled) {
            channel.value = readSingleChannel(channel);
        } else {
            channel.value = 0;
        }
    }
}

// Build JSON response and return CSV line
String buildJsonResponse(String& csvLine) {
    const float ADC_TO_VOLT = 2.048 / 32768.0;

    float adc_volt_1 = adcChannels[0].value * ADC_TO_VOLT;
    float adc_volt_3 = adcChannels[1].value * ADC_TO_VOLT;
    float adc_volt_4 = adcChannels[2].value * ADC_TO_VOLT;

    float voltage_1 = adcChannels[0].enabled ? 
        (adc_volt_1 <= 0.966 ? (adc_volt_1 * 11.03 - 0.01) : 
         (adc_volt_1 <= 1.389 ? (adc_volt_1 * 11.75 - 0.95) : (adc_volt_1 * 12.05 - 1.65))) : 0.0;
    float voltage_3 = adcChannels[1].enabled ? 
        (adc_volt_3 <= 0.966 ? (adc_volt_3 * 11.03 - 0.01) : 
         (adc_volt_3 <= 1.389 ? (adc_volt_3 * 11.75 - 0.95) : (adc_volt_3 * 12.05 - 1.65))) : 0.0;
    float voltage_4 = adcChannels[2].enabled ? 
        (adc_volt_4 <= 0.966 ? (adc_volt_4 * 11.03 - 0.01) : 
         (adc_volt_4 <= 1.389 ? (adc_volt_4 * 11.75 - 0.95) : (adc_volt_4 * 12.05 - 1.65))) : 0.0;

    float current_1 = adcChannels[3].enabled ? (adcChannels[3].value * ADC_TO_VOLT) : 0.0;
    float current_2 = adcChannels[4].enabled ? (adcChannels[4].value * ADC_TO_VOLT) : 0.0;

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "{\"time\":\"%s\",\"voltage_1\":%.3f,\"voltage_3\":%.3f,\"voltage_4\":%.3f,\"current_1\":%.3f,\"current_2\":%.3f,"
             "\"raw_adc_1\":%d,\"raw_adc_3\":%d,\"raw_adc_4\":%d}",
             getTime().c_str(), voltage_1, voltage_3, voltage_4, current_1, current_2,
             adcChannels[0].value, adcChannels[1].value, adcChannels[2].value);

    char csvBuffer[128];
    snprintf(csvBuffer, sizeof(csvBuffer),
             "%s,%.3f,%.3f,%.3f,%.3f,%.3f",
             getTime().c_str(), voltage_1, voltage_3, voltage_4, current_1, current_2);
    csvLine = String(csvBuffer);

    return String(buffer);
}
// Log data to CSV file
void logToCsv(const String& csvLine) {
    File file = SPIFFS.open(LOG_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open log file for appending");
        return;
    }
    if (file.size() == 0) {
        file.println("time,voltage_1,voltage_3,voltage_4,current_1,current_2");
    }
    file.println(csvLine);
    file.close();
}

// Delete log file
void deleteLogFile() {
    if (SPIFFS.exists(LOG_FILE)) {
        if (SPIFFS.remove(LOG_FILE)) {
            Serial.printf("Deleted %s\n", LOG_FILE);
        } else {
            Serial.printf("Failed to delete %s\n", LOG_FILE);
        }
    }
}

void listFiles() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.printf("File: %s Size: %u\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

void displayStorageCapacity() {
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    Serial.println("\nSPIFFS Storage Capacity:");
    Serial.printf("Total space: %u bytes\n", totalBytes);
    Serial.printf("Used space: %u bytes\n", usedBytes);
    Serial.printf("Free space: %u bytes\n", totalBytes - usedBytes);
    Serial.printf("Usage: %.1f%%\n", (float)usedBytes / totalBytes * 100.0);
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

String getTime() {
    time_t now = time(nullptr);
    if (now < 100000) return "No NTP sync";
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

void configureWiFi(WiFiManager& wifiManager) {
    if (!wifiManager.autoConnect("ESP32-Setup")) {
        Serial.println("Failed to connect. Rebooting...");
        ESP.restart();
    }
    wifiConnected = true;
    Serial.printf("Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());
}

// Function to connect to an enterprise WiFi network (WPA2-Enterprise)
void configureEnterpriseWiFi(const char* ssid, const char* username, const char* password) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    Serial.printf("Attempting to connect to Enterprise SSID: %s\n", ssid);
    
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();

    WiFi.begin(ssid);

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\nConnected to Enterprise WiFi! IP Address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifiConnected = false;
        Serial.println("\nFailed to connect to Enterprise WiFi.");
    }
}

void setupWebServer() {
    server.on("/", HTTP_GET, []() { serveFile("/index.html", "text/html"); });
    server.on("/style.css", HTTP_GET, []() { serveFile("/style.css", "text/css"); });
    server.on("/script.js", HTTP_GET, []() { serveFile("/script.js", "application/javascript"); });
    server.on("/logo.png", HTTP_GET, []() { serveFile("/logo.png", "image/png"); });
    
    server.on("/download-log", HTTP_GET, []() {
        if (SPIFFS.exists(LOG_FILE)) {
            File file = SPIFFS.open(LOG_FILE, "r");
            server.streamFile(file, "text/csv");
            file.close();
        } else {
            server.send(404, "text/plain", "Log file not found");
        }
    });

    server.on("/get-channels", HTTP_GET, []() {
        String json = "[";
        for (size_t i = 0; i < sizeof(adcChannels) / sizeof(adcChannels[0]); i++) {
            json += "{\"name\":\"" + String(adcChannels[i].name) + "\",\"enabled\":" + (adcChannels[i].enabled ? "true" : "false") + "}";
            if (i < (sizeof(adcChannels) / sizeof(adcChannels[0]) - 1)) json += ",";
        }
        json += "]";
        server.send(200, "application/json", json);
    });

    server.on("/set-channel", HTTP_POST, []() {
        if (server.hasArg("name") && server.hasArg("enabled")) {
            String name = server.arg("name");
            bool enabled = server.arg("enabled") == "true";
            for (auto& channel : adcChannels) {
                if (name == channel.name) {
                    channel.enabled = enabled;
                    Serial.printf("Channel %s set to %s\n", name.c_str(), enabled ? "enabled" : "disabled");
                    break;
                }
            }
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid request");
        }
    });

    server.on("/get-wifi-config", HTTP_GET, []() {
        WifiConfig config = loadWifiConfig();
        String json = "{";
        json += "\"useEnterprise\":" + String(config.useEnterprise ? "true" : "false") + ",";
        json += "\"enterpriseSsid\":\"" + config.enterpriseSsid + "\",";
        json += "\"enterpriseUsername\":\"" + config.enterpriseUsername + "\",";
        json += "\"enterprisePassword\":\"" + config.enterprisePassword + "\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/set-wifi-config", HTTP_POST, []() {
        if (server.hasArg("useEnterprise") && server.hasArg("enterpriseSsid") && server.hasArg("enterpriseUsername") && server.hasArg("enterprisePassword")) {
            WifiConfig config;
            config.useEnterprise = server.arg("useEnterprise") == "true";
            config.enterpriseSsid = server.arg("enterpriseSsid");
            config.enterpriseUsername = server.arg("enterpriseUsername");
            config.enterprisePassword = server.arg("enterprisePassword");
            saveWifiConfig(config);
            server.send(200, "text/plain", "WiFi configuration updated. Please restart the device to apply changes.");
        } else {
            server.send(400, "text/plain", "Invalid request");
        }
    });

    server.begin();
    Serial.printf("HTTP server started at: http://%s\n", WiFi.localIP().toString().c_str());
}

void setupWebSocket() {
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
}

void setup() {
    Serial.begin(115200);
    Serial.println("Booting ESP32...");

    Wire.begin(SDA_PIN, SCL_PIN);
    Serial.println("Initializing MCP3426 ADCs...");

    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");

    WifiConfig wifiConfig = loadWifiConfig();

    if (wifiConfig.useEnterprise && wifiConfig.enterpriseSsid != "" && wifiConfig.enterpriseUsername != "" && wifiConfig.enterprisePassword != "") {
        configureEnterpriseWiFi(wifiConfig.enterpriseSsid.c_str(), wifiConfig.enterpriseUsername.c_str(), wifiConfig.enterprisePassword.c_str());
    }

    if (!wifiConnected) {
        WiFiManager wifiManager;
        configureWiFi(wifiManager);
    }

    pinMode(LED_PIN, OUTPUT);

    deleteLogFile();
    listFiles();
    displayStorageCapacity();

    configTime(0, 0, "pool.ntp.org");
    Serial.println("Waiting for NTP sync...");
    delay(5000);
    Serial.printf("Current time: %s\n", getTime().c_str());

    esp_task_wdt_init(WDT_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    setupWebSocket();
    setupWebServer();
}

void loop() {
    esp_task_wdt_reset();
    webSocket.loop();
    server.handleClient();

    static uint32_t lastLedToggle = 0;
    static uint32_t lastRead = 0;
    uint32_t now = millis();

    if (wifiConnected && now - lastLedToggle >= LED_BLINK_INTERVAL_MS) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        lastLedToggle = now;
    }

    if (now - lastRead >= ADC_READ_INTERVAL_MS) {
        updateAdcValues();
        if (webSocket.connectedClients() > 0) {
            String csvLine;
            String jsonResponse = buildJsonResponse(csvLine);
            Serial.printf("Broadcasting: %s\n", jsonResponse.c_str());
            webSocket.broadcastTXT(jsonResponse);
            logToCsv(csvLine);
        }
        lastRead = now;
    }
}