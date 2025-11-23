// server/cam_grabber/hikvision_camera.cpp
#include "hikvision_camera.h"
#include <chrono>
#include <cstring>
#include <iostream>

HikvisionCamera::HikvisionCamera(const std::string& name,
                                 const std::string& serialNumber,
                                 uint32_t width,
                                 uint32_t height)
    : m_name(name)
    , m_serial(serialNumber)
    , m_width(width)
    , m_height(height)
{
}

HikvisionCamera::~HikvisionCamera()
{
    stop();
}

bool HikvisionCamera::openDeviceBySerial()
{
    MV_CC_DEVICE_INFO_LIST devList{};
    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &devList);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] EnumDevices failed, nRet=" << nRet << std::endl;
        return false;
    }

    MV_CC_DEVICE_INFO* found = nullptr;
    for (unsigned int i = 0; i < devList.nDeviceNum; ++i) {
        MV_CC_DEVICE_INFO* devInfo = devList.pDeviceInfo[i];
        if (!devInfo) continue;

        std::string serial;
        if (devInfo->nTLayerType == MV_GIGE_DEVICE) {
            MV_GIGE_DEVICE_INFO* gigeInfo =
                reinterpret_cast<MV_GIGE_DEVICE_INFO*>(devInfo->SpecialInfo.stGigEInfo);
            serial = gigeInfo->chSerialNumber;
        } else if (devInfo->nTLayerType == MV_USB_DEVICE) {
            MV_USB3_DEVICE_INFO* usbInfo =
                reinterpret_cast<MV_USB3_DEVICE_INFO*>(devInfo->SpecialInfo.stUsb3VInfo);
            serial = usbInfo->chSerialNumber;
        }

        if (serial == m_serial) {
            found = devInfo;
            break;
        }
    }

    if (!found) {
        std::cerr << "[Hik] Device with serial " << m_serial << " not found" << std::endl;
        return false;
    }

    m_devInfo = found;

    nRet = MV_CC_CreateHandle(&m_handle, m_devInfo);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] CreateHandle failed, nRet=" << nRet << std::endl;
        m_handle = nullptr;
        return false;
    }

    nRet = MV_CC_OpenDevice(m_handle);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] OpenDevice failed, nRet=" << nRet << std::endl;
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    return true;
}

bool HikvisionCamera::applyBasicSettings()
{
    if (!m_handle) return false;

    int nRet = MV_OK;

    // 1) TriggerMode Off (연속 그랩)
    nRet = MV_CC_SetEnumValue(m_handle, "TriggerMode", 0);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] Set TriggerMode Off failed, nRet=" << nRet << std::endl;
    }

    // 2) PixelFormat 설정 (예: Mono8 or BGR8)
    //   - 실제 센서 포맷에 맞게 선택
    // nRet = MV_CC_SetEnumValue(m_handle, "PixelFormat", PixelType_Gvsp_Mono8);
    // 또는 BGR8
    // nRet = MV_CC_SetEnumValue(m_handle, "PixelFormat", PixelType_Gvsp_BGR8_Packed);

    // 3) Width / Height (ROI 전체를 640x480으로 맞춘다고 가정)
    nRet = MV_CC_SetIntValue(m_handle, "Width", m_width);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] Set Width failed, nRet=" << nRet << std::endl;
    }
    nRet = MV_CC_SetIntValue(m_handle, "Height", m_height);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] Set Height failed, nRet=" << nRet << std::endl;
    }

    // 4) Exposure / Gain 등 필요한 최소 설정
    // MV_CC_SetFloatValue(m_handle, "ExposureTime", 5000.0f);
    // MV_CC_SetFloatValue(m_handle, "Gain", 10.0f);

    return true;
}

bool HikvisionCamera::start()
{
    if (m_running.load()) return true;

    if (!openDeviceBySerial()) {
        std::cerr << "[Hik] openDeviceBySerial failed: " << m_name << std::endl;
        return false;
    }

    if (!applyBasicSettings()) {
        std::cerr << "[Hik] applyBasicSettings failed: " << m_name << std::endl;
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // 콜백 등록
    int nRet = MV_CC_RegisterImageCallBackEx(
        m_handle,
        HikvisionCamera::imageCallback,
        this
    );
    if (MV_OK != nRet) {
        std::cerr << "[Hik] RegisterImageCallBackEx failed, nRet=" << nRet << std::endl;
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // 그랩 시작
    nRet = MV_CC_StartGrabbing(m_handle);
    if (MV_OK != nRet) {
        std::cerr << "[Hik] StartGrabbing failed, nRet=" << nRet << std::endl;
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_running.store(true);
    std::cout << "[HikvisionCamera] start: " << m_name << " (" << m_serial << ")" << std::endl;
    return true;
}

void HikvisionC
