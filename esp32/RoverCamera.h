#ifndef ROVER_CAMERA_H
#define ROVER_CAMERA_H

#include "esp_camera.h"

class RoverCamera {
public:
    RoverCamera();
    bool init();
    camera_fb_t* captureFrame();
    void releaseFrame(camera_fb_t* fb);
};

#endif