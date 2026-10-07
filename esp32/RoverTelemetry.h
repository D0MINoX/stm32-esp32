#ifndef ROVER_TELEMETRY_H
#define ROVER_TELEMETRY_H

#include <Arduino.h>
#include <WiFi.h>
class RoverTelemetry {
public:
    RoverTelemetry(int tcpPort, long uartBaudRate);
    void init();
    void processTunnel();
    float getESPTemperature();

private:
    static const uint8_t MOTOR_PACKET_SIZE = 4;

    WiFiServer _server;
    long _baudRate;
    int _port;
    WiFiClient _client;

    uint8_t _packetBuffer[MOTOR_PACKET_SIZE];
    uint8_t _packetIndex = 0;
};

#endif