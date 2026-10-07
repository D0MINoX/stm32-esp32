#include "RoverCamera.h"
#include <Arduino.h>

RoverCamera::RoverCamera() {}

bool RoverCamera::init() {
    camera_config_t config;
    
    // Pinout dla XIAO ESP32S3 Sense
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 15;
    config.pin_d1 = 17;
    config.pin_d2 = 18;
    config.pin_d3 = 16;
    config.pin_d4 = 14;
    config.pin_d5 = 12;
    config.pin_d6 = 11;
    config.pin_d7 = 48;
    config.pin_xclk = 10;
    config.pin_pclk = 13;
    config.pin_vsync = 38;
    config.pin_href = 47;
    config.pin_sccb_sda = 40;
    config.pin_sccb_scl = 39;
    config.pin_pwdn = -1;
    config.pin_reset = -1;

    // Konfiguracja matrycy wideo
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_HD;       // 720p
    config.jpeg_quality = 10;                // Kompresja (10-12 to dobry kompromis)
    config.fb_count = 3;                     // Buforujemy 3 klatki
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM; // Zapis do zewnętrznego RAM-u

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Blad kamery: 0x%x\n", err);
        return false;
    }
    return true;
}

camera_fb_t* RoverCamera::captureFrame() {
    // Zwraca wskaźnik do surowej klatki w pamięci
    return esp_camera_fb_get();
}

void RoverCamera::releaseFrame(camera_fb_t* fb) {
    // Oddaje pamięć z powrotem do układu (zapobiega wyciekom)
    if (fb != nullptr) {
        esp_camera_fb_return(fb);
    }
}