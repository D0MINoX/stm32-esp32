#include "RoverTelemetry.h"

RoverTelemetry::RoverTelemetry(int tcpPort, long uartBaudRate)
: _server(tcpPort), _baudRate(uartBaudRate), _port(tcpPort) {}

void RoverTelemetry::init() {
    Serial1.begin(_baudRate, SERIAL_8N1, 44, 43);
    Serial1.setTimeout(10);
    _server.begin();

    Serial.print("Tunel TCP <-> UART uruchomiony na porcie: ");
    Serial.println(_port);
}
float RoverTelemetry::getESPTemperature() {
   return temperatureRead();
}
void RoverTelemetry::processTunnel() {
    if (!_client || !_client.connected()) {
        _client = _server.available();
        if (_client && _client.connected()) {
            Serial.println("[TCP] Klient podlaczony");
            _packetIndex = 0;
        }
    }

    if (_client && _client.connected()) {
        // 1. TCP -> UART (Sterowanie z pada) - ZAKOMENTOWANO SERIAL.PRINT
        while (_client.available()) {
            uint8_t b = (uint8_t)_client.read();
            _packetBuffer[_packetIndex++] = b;

            if (_packetIndex >= MOTOR_PACKET_SIZE) {
                // Usunięto pętlę for z Serial.print(HEX) - to bardzo przyspieszy tunel!
                Serial1.write(_packetBuffer, MOTOR_PACKET_SIZE);
                Serial1.flush();
                _packetIndex = 0;
            }
        }

        // 2. UART -> TCP (Telemetria z STM32) - OPTYMALIZACJA BUFORA
        uint8_t uartBuffer[64];
        size_t bytesRead = 0;

        // Czytamy tyle bajtów, ile aktualnie czeka w sprzętowym buforze UART
        if (Serial1.available()) {
            String stm_data = Serial1.readStringUntil('\n'); 

            if (stm_data.length() > 0 && stm_data.startsWith("{")) {
                float esp_temp = temperatureRead();
                String final_json = stm_data + String(esp_temp, 2) + "}\n";

                _client.print(final_json);
            }
        }
    } else {
        // Czyszczenie bufora, gdy brak połączenia
        while (Serial1.available()) {
            Serial1.read();
        }
        _packetIndex = 0;
    }
}