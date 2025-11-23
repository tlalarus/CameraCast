#ifndef CAMERA_CAST_CAMERA_DEVICE_H
#define CAMERA_CAST_CAMERA_DEVICE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct Frame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
};

using FrameCallback = std::function<void(const Frame &)>;

class CameraDevice {
public:
    virtual ~CameraDevice() = default;

    virtual bool start(FrameCallback callback) = 0;
    virtual void stop() = 0;
    virtual std::string name() const = 0;
};

#endif  // CAMERA_CAST_CAMERA_DEVICE_H
