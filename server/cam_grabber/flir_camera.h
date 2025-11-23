// server/cam_grabber/flir_camera.h
#pragma once
#include "camera_device.h"
#include <atomic>
#include <string>

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

class FlirCamera : public ICameraDevice
{
public:
    FlirCamera(const std::string& name,
               const std::string& serialNumber,
               uint32_t width,
               uint32_t height);

    ~FlirCamera() override;

    bool start() override;
    void stop() override;
    bool getLatestFrame(Frame& outFrame) override;
    std::string name() const override { return m_name; }

private:
    std::string m_name;
    std::string m_serial;
    uint32_t m_width;
    uint32_t m_height;

    std::mutex m_mutex;
    Frame m_latestFrame;
    std::atomic<bool> m_running{false};

    Spinnaker::SystemPtr m_system;
    Spinnaker::CameraPtr m_camera;

    class ImageEventHandler;
    ImageEventHandler* m_imageEventHandler = nullptr;

    bool openDeviceBySerial();
    bool applyBasicSettings();

    friend class ImageEventHandler;
};
