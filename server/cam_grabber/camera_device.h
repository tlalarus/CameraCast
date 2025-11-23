#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class PixelFormat : uint8_t {
    kJpeg = 1,
    kRaw = 2,
};

struct Frame {
    PixelFormat pixel_format = PixelFormat::kJpeg;
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint64_t timestamp_us = 0;
};

using FrameCallback = std::function<void(const Frame &)>;

class CameraDevice {
public:
    virtual ~CameraDevice() = default;

    virtual bool start(FrameCallback callback) = 0;
    virtual void stop() = 0;
    virtual std::string name() const = 0;
};
