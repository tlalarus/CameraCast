// server/cam_grabber/flir_camera.cpp
#include "flir_camera.h"
#include <chrono>
#include <iostream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;

class FlirCamera::ImageEventHandler : public Spinnaker::ImageEventHandler
{
public:
    explicit ImageEventHandler(FlirCamera* owner) : m_owner(owner) {}

    void OnImageEvent(ImagePtr image) override
    {
        if (!m_owner || !m_owner->m_running.load() || !image || !image->IsValid())
            return;

        Frame frame;
        frame.width  = static_cast<uint32_t>(image->GetWidth());
        frame.height = static_cast<uint32_t>(image->GetHeight());

        size_t dataSize = image->GetBufferSize();
        unsigned char* pData = static_cast<unsigned char*>(image->GetData());
        frame.data.assign(pData, pData + dataSize);

        auto now = std::chrono::steady_clock::now().time_since_epoch();
        frame.timestamp_us =
            std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        {
            std::lock_guard<std::mutex> lock(m_owner->m_mutex);
            m_owner->m_latestFrame = std::move(frame);
        }
    }

private:
    FlirCamera* m_owner = nullptr;
};

FlirCamera::FlirCamera(const std::string& name,
                       const std::string& serialNumber,
                       uint32_t width,
                       uint32_t height)
    : m_name(name)
    , m_serial(serialNumber)
    , m_width(width)
    , m_height(height)
{
}

FlirCamera::~FlirCamera()
{
    stop();
}

bool FlirCamera::openDeviceBySerial()
{
    m_system = System::GetInstance();
    CameraList camList = m_system->GetCameras();

    CameraPtr foundCam = nullptr;

    for (unsigned int i = 0; i < camList.GetSize(); ++i) {
        CameraPtr cam = camList.GetByIndex(i);
        INodeMap& nodeMapTL = cam->GetTLDeviceNodeMap();
        CStringPtr ptrSerial = nodeMapTL.GetNode("DeviceSerialNumber");
        if (IsAvailable(ptrSerial) && IsReadable(ptrSerial)) {
            std::string serial = ptrSerial->GetValue();
            if (serial == m_serial) {
                foundCam = cam;
                break;
            }
        }
    }

    if (!foundCam) {
        std::cerr << "[FLIR] Device with serial " << m_serial << " not found" << std::endl;
        camList.Clear();
        m_system->ReleaseInstance();
        m_system = nullptr;
        return false;
    }

    m_camera = foundCam;
    camList.Clear();

    m_camera->Init();
    return true;
}

bool FlirCamera::applyBasicSettings()
{
    if (!m_camera) return false;

    INodeMap& nodeMap = m_camera->GetNodeMap();

    // 1) AcquisitionMode = Continuous
    CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
    if (IsAvailable(ptrAcquisitionMode) && IsWritable(ptrAcquisitionMode)) {
        CEnumEntryPtr ptrModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (IsAvailable(ptrModeContinuous) && IsReadable(ptrModeContinuous)) {
            int64_t val = ptrModeContinuous->GetValue();
            ptrAcquisitionMode->SetIntValue(val);
        }
    }

    // 2) PixelFormat (예: Mono8 또는 BGR8)
    CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
    if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat)) {
        // 예: Mono8
        CEnumEntryPtr ptrMono8 = ptrPixelFormat->GetEntryByName("Mono8");
        if (IsAvailable(ptrMono8) && IsReadable(ptrMono8)) {
            int64_t val = ptrMono8->GetValue();
            ptrPixelFormat->SetIntValue(val);
        }
    }

    // 3) Width/Height (ROI를 2656x2304로 맞춘다고 가정)
    CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
    CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
    if (IsAvailable(ptrWidth) && IsWritable(ptrWidth)) {
        ptrWidth->SetValue(m_width);
    }
    if (IsAvailable(ptrHeight) && IsWritable(ptrHeight)) {
        ptrHeight->SetValue(m_height);
    }

    // 4) Exposure / Gain 등 최소 설정
    // CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
    // if (IsAvailable(ptrExposureTime) && IsWritable(ptrExposureTime)) {
    //     ptrExposureTime->SetValue(5000.0);
    // }

    return true;
}

bool FlirCamera::start()
{
    if (m_running.load()) return true;

    if (!openDeviceBySerial()) {
        std::cerr << "[FLIR] openDeviceBySerial failed: " << m_name << std::endl;
        return false;
    }

    if (!applyBasicSettings()) {
        std::cerr << "[FLIR] applyBasicSettings failed: " << m_name << std::endl;
        m_camera->DeInit();
        m_camera = nullptr;
        m_system->ReleaseInstance();
        m_system = nullptr;
        return false;
    }

    m_imageEventHandler = new ImageEventHandler(this);
    m_camera->RegisterEventHandler(*m_imageEventHandler);

    m_camera->BeginAcquisition();

    m_running.store(true);
    std::cout << "[FlirCamera] start: " << m_name << " (" << m_serial << ")" << std::endl;
    return true;
}

void FlirCamera::stop()
{
    if (!m_running.exchange(false)) return;

    if (m_camera) {
        try {
            m_camera->EndAcquisition();
        } catch (...) {}

        if (m_imageEventHandler) {
            m_camera->UnregisterEventHandler(*m_imageEventHandler);
            delete m_imageEventHandler;
            m_imageEventHandler = nullptr;
        }

        try {
            m_camera->DeInit();
        } catch (...) {}

        m_camera = nullptr;
    }

    if (m_system) {
        m_system->ReleaseInstance();
        m_system = nullptr;
    }

    std::cout << "[FlirCamera] stop: " << m_name << std::endl;
}

bool FlirCamera::getLatestFrame(Frame& outFrame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_latestFrame.data.empty()) return false;
    outFrame = m_latestFrame;
    return true;
}
