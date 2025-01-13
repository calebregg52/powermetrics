#include <WiFiManager.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <FS.h>

bool WIFI_CONNECT_FLAG = false;
#define LED_BUILTIN 2
WebServer server(80);

void listFiles() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.print("File: ");
    Serial.print(file.name());
    Serial.print(" Size: ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
}

void setup() {
  Serial.begin(115200); //begin serial output
  WiFiManager wifiManager; //wifimanager handler

  //Reset settings
  wifiManager.resetSettings();

  // Start the captive portal
  if (!wifiManager.autoConnect("ESP32-Setup")) {
    Serial.println("Failed to connect. Rebooting...");
    ESP.restart();
  }

  // Connected to Wi-Fi
  Serial.println("Connected! IP Address: " + WiFi.localIP().toString());
  pinMode(LED_BUILTIN, OUTPUT);
  WIFI_CONNECT_FLAG = true;

  if (SPIFFS.begin(true)) {

    Serial.println("SPIFFS mounted successfully");
    listFiles();  // List all files in SPIFFS to verify
  } else {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  
  server.on("/", HTTP_GET, []() {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            return;
        }
        server.streamFile(file, "text/html");
        file.close();
    });

    // Start the server
    server.begin();
    Serial.println("Server started");
    Serial.print("ESP32 is hosting at: http://");
    Serial.println(WiFi.localIP());


}

void loop() {

  if(WIFI_CONNECT_FLAG == true){
    digitalWrite(LED_BUILTIN, HIGH);  
    delay(500);                      
    digitalWrite(LED_BUILTIN, LOW);   
    delay(500);  
  }

  server.handleClient();

}
