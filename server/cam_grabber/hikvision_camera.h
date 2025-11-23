// server/cam_grabber/hikvision_camera.h
#pragma once
#include "camera_device.h"
#include <atomic>
#include <string>

// Hikvision MVS SDK
#include "MvCameraControl.h"   // 실제 설치 경로에 맞게 include

class HikvisionCamera : public ICameraDevice
{
public:
    HikvisionCamera(const std::string& name,
                    const std::string& serialNumber,
                    uint32_t width,
                    uint32_t height);

    ~HikvisionCamera() override;

    bool start() override;
    void stop() override;
    bool getLatestFrame(Frame& outFrame) override;
    std::string name() const override { return m_name; }

private:
    std::string m_name;
    std::string m_serial;
    uint32_t m_width;
    uint32_t m_height;

    MV_CC_DEVICE_INFO* m_devInfo = nullptr;
    void* m_handle = nullptr;               // MV_CC_DEVICE_HANDLE 이 void* typedef

    std::mutex m_mutex;
    Frame m_latestFrame;
    std::atomic<bool> m_running{false};

    static void __stdcall imageCallback(unsigned char* pData,
                                        MV_FRAME_OUT_INFO_EX* pFrameInfo,
                                        void* pUser);

    void handleCallback(unsigned char* pData,
                        MV_FRAME_OUT_INFO_EX* pFrameInfo);

    bool openDeviceBySerial();
    bool applyBasicSettings();
};