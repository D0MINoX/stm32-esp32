#ifndef ROVER_STREAMER_H
#define ROVER_STREAMER_H

#include <Arduino.h>
#include <WiFi.h>

class RoverStreamer {
public:
    RoverStreamer(const char* ssid, const char* password);
    void configStaticIP(IPAddress ip, IPAddress gateway, IPAddress subnet);
    void connectWiFi();
    WiFiServer& getServer(); 
    
private:
    const char* _ssid;
    const char* _password;
    WiFiServer _server;
    
    bool _useStaticIP;
    IPAddress _ip;
    IPAddress _gateway;
    IPAddress _subnet;
};

#endif