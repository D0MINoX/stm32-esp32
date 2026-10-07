#include <Arduino.h>
#include "RoverCamera.h"
#include "RoverStreamer.h"
#include "RoverTelemetry.h" // --- DODANE ---

// UWAGA: Wpisz tutaj dane do swojego domowego Wi-Fi lub hotspota z telefonu!
const char* WIFI_SSID = "DESKTOP-VC114AT 6270";
const char* WIFI_PASS = "0Q9-a903";

RoverCamera cam;
RoverStreamer streamer(WIFI_SSID, WIFI_PASS);

// --- DODANE: Tworzymy tunel na porcie 8080 z prędkością UART 115200 ---
RoverTelemetry telemetry(8080, 115200); 

// Kolejka FreeRTOS przechowująca wskaźniki do zdjęć
QueueHandle_t frameQueue;

// Deklaracje zadań
void taskCapture(void *pvParameters);
void taskStream(void *pvParameters);
void taskTelemetryTunnel(void *pvParameters); // --- DODANE ---

void setup() {
    Serial.begin(115200);
    
    // Zabezpieczenie przed uśpionym terminalem
    
    
    Serial.println("\n\n=================================");
    Serial.println("--- START SYSTEMU LAZIKA ---");
    Serial.println("=================================\n");

    if (!cam.init()) {
        Serial.println("Kamera uszkodzona / zle podpieta. Zatrzymuje system.");
        while (true) { delay(100); }
    }

    // Łączenie z Wi-Fi
    streamer.connectWiFi();
    
    // --- DODANE: Uruchomienie tunelu UART ---
    telemetry.init();

    // Tworzymy kolejkę zdolną pomieścić 2 wskaźniki klatek naraz
    frameQueue = xQueueCreate(2, sizeof(camera_fb_t*));

    // Rdzeń 1: Robi zdjęcia (Hardware)
    xTaskCreatePinnedToCore(taskCapture, "CameraTask", 4096, NULL, 1, NULL, 1);
    
    // Rdzeń 0: Wystawia serwer kamery (Sieć)
    xTaskCreatePinnedToCore(taskStream, "StreamTask", 8192, NULL, 1, NULL, 0);
    
    // --- DODANE: Rdzeń 0 obsługuje też tunel sterowania (Sieć) ---
    xTaskCreatePinnedToCore(taskTelemetryTunnel, "TunnelTask", 4096, NULL, 2, NULL, 0);
}

void loop() {
    // FreeRTOS zarządza wszystkim w tle, usuwamy domyślną pętlę
    vTaskDelete(NULL);
}

// ==========================================
// ZADANIE RDZENIA 1 (Robienie zdjęć)
// ==========================================
void taskCapture(void *pvParameters) {
    while (true) {
        camera_fb_t* fb = cam.captureFrame();
        if (!fb) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (xQueueSend(frameQueue, &fb, 10) != pdPASS) {
            cam.releaseFrame(fb);
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}

// ==========================================
// ZADANIE RDZENIA 0 (Wysyłanie przez Wi-Fi)
// ==========================================
void taskStream(void *pvParameters) {
    WiFiServer& server = streamer.getServer();
    server.begin(); 

    while (true) {
        WiFiClient client = server.available();
        
        if (client) {
            Serial.println("Ktos podlaczyl sie do kamery!");
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
            client.println();

            while (client.connected()) {
                camera_fb_t* fb = nullptr;
                
                if (xQueueReceive(frameQueue, &fb, 100/portTICK_PERIOD_MS) == pdPASS) {
                    client.println("--frame");
                    client.println("Content-Type: image/jpeg");
                    client.printf("Content-Length: %u\r\n\r\n", fb->len);
                    client.write(fb->buf, fb->len);
                    client.println();
                    cam.releaseFrame(fb);
                }else {
                    // Jeśli przez 100ms nie było nowej klatki, oddaj zasoby, żeby nie zamrozić rdzenia
                    vTaskDelay(5 / portTICK_PERIOD_MS);
                }
            }
            client.stop();
            Serial.println("Klient oglądający odłączony.");
        } else {
            camera_fb_t* fb = nullptr;
            if (xQueueReceive(frameQueue, &fb, 10 / portTICK_PERIOD_MS) == pdPASS) {
                cam.releaseFrame(fb);
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

// ==========================================
// ZADANIE RDZENIA 0 (Tunel TCP-UART)
// ==========================================
void taskTelemetryTunnel(void *pvParameters) {
    while (true) {
        telemetry.processTunnel();
        // Oddajemy procesor na 10 ms, żeby nie zablokować streamowania wideo!
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}