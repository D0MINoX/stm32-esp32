#include "RoverStreamer.h"

RoverStreamer::RoverStreamer(const char* ssid, const char* password) 
    : _ssid(ssid), _password(password), _server(80), _useStaticIP(false) {}

void RoverStreamer::configStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet) {
    _ip = ip;
    _gateway = gateway;
    _subnet = subnet;
    _useStaticIP = true;
}

void RoverStreamer::connectWiFi() {
    // Jeśli zdefiniowano statyczne IP, konfigurujemy moduł przed połączeniem
    if (_useStaticIP) {
        if (!WiFi.config(_ip, _gateway, _subnet)) {
            Serial.println("Blad konfiguracji statycznego IP!");
        }
    }

    WiFi.begin(_ssid, _password);
    Serial.print("Laczenie z Wi-Fi");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nPolaczono!");
    Serial.print("Adres kamery lazika: http://");
    Serial.println(WiFi.localIP());
}

WiFiServer& RoverStreamer::getServer() {
    return _server;
}