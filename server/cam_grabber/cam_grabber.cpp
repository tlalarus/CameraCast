#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "camera_device.h"
#include "flir_camera.h"
#include "frame_bundle.h"
#include "hikvision_camera.h"

namespace {

#pragma pack(push, 1)
struct UdsPacketHeader {
    uint32_t magic;
    uint32_t size;
};
#pragma pack(pop)

constexpr uint32_t kUdsMagic = 0xDEADBEEF;
constexpr char kSocketPath[] = "/tmp/cam_stream.sock";
constexpr std::size_t kMaxCameraCount = 3;

uint64_t currentTimestampUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct ManagedCamera {
    uint8_t id;
    std::unique_ptr<CameraDevice> device;
};

bool transmitBundle(int sock,
                    uint64_t seq,
                    const std::vector<std::pair<uint8_t, Frame>> &frames,
                    size_t &bytes_sent) {
    if (frames.empty()) {
        return false;
    }

    FrameBundleHeader bundle{};
    bundle.magic = FB_MAGIC;
    bundle.version = 1;
    bundle.camera_count = static_cast<uint16_t>(frames.size());
    bundle.seq = seq;
    bundle.timestamp_us = currentTimestampUs();

    size_t estimated_size = sizeof(FrameBundleHeader);
    for (const auto &entry : frames) {
        estimated_size += sizeof(CameraFrameHeader) + entry.second.data.size();
    }

    std::vector<uint8_t> payload;
    payload.reserve(estimated_size);
    payload.insert(payload.end(),
                   reinterpret_cast<const uint8_t *>(&bundle),
                   reinterpret_cast<const uint8_t *>(&bundle) + sizeof(bundle));

    for (const auto &entry : frames) {
        const auto camera_id = entry.first;
        const auto &frame = entry.second;

        CameraFrameHeader frame_header{};
        frame_header.camera_id = camera_id;
        frame_header.pixel_format = static_cast<uint8_t>(frame.pixel_format);
        frame_header.reserved = 0;
        frame_header.width = frame.width;
        frame_header.height = frame.height;
        frame_header.data_size = static_cast<uint32_t>(frame.data.size());

        payload.insert(payload.end(),
                       reinterpret_cast<const uint8_t *>(&frame_header),
                       reinterpret_cast<const uint8_t *>(&frame_header) + sizeof(frame_header));
        payload.insert(payload.end(), frame.data.begin(), frame.data.end());
    }

    UdsPacketHeader uds_header{};
    uds_header.magic = kUdsMagic;
    uds_header.size = static_cast<uint32_t>(payload.size());

    std::vector<uint8_t> packet;
    packet.reserve(sizeof(uds_header) + payload.size());
    packet.insert(packet.end(),
                  reinterpret_cast<const uint8_t *>(&uds_header),
                  reinterpret_cast<const uint8_t *>(&uds_header) + sizeof(uds_header));
    packet.insert(packet.end(), payload.begin(), payload.end());

    size_t total_sent = 0;
    while (total_sent < packet.size()) {
        const ssize_t sent = ::send(sock,
                                    packet.data() + total_sent,
                                    packet.size() - total_sent,
                                    0);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }

    bytes_sent = total_sent;
    return true;
}

}  // namespace

int main() {
    int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path) - 1);

    if (::connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        ::close(sock);
        return 1;
    }

    std::cout << "Connected to UNIX socket: " << kSocketPath << std::endl;

    std::vector<ManagedCamera> cameras;
    cameras.push_back(ManagedCamera{0, std::make_unique<HikvisionCamera>("FrontGate")});
    cameras.push_back(ManagedCamera{1, std::make_unique<HikvisionCamera>("Warehouse")});
    cameras.push_back(ManagedCamera{2, std::make_unique<FlirCamera>("Thermal01")});

    std::array<Frame, kMaxCameraCount> latest_frames;
    std::array<bool, kMaxCameraCount> has_frame{};
    std::mutex frame_mutex;

    std::vector<uint8_t> active_camera_ids;
    active_camera_ids.reserve(cameras.size());

    for (auto &camera : cameras) {
        if (camera.id >= kMaxCameraCount) {
            std::cerr << "Camera id " << static_cast<int>(camera.id)
                      << " exceeds collector capacity" << std::endl;
            continue;
        }

        auto callback = [&, id = camera.id](const Frame &frame) {
            std::lock_guard<std::mutex> lock(frame_mutex);
            latest_frames[id] = frame;
            has_frame[id] = true;
        };

        if (camera.device->start(callback)) {
            std::cout << "Started camera: " << camera.device->name() << std::endl;
            active_camera_ids.push_back(camera.id);
        } else {
            std::cerr << "Failed to start camera: " << camera.device->name() << std::endl;
        }
    }

    if (active_camera_ids.empty()) {
        std::cerr << "No cameras running. Exiting." << std::endl;
        ::close(sock);
        return 1;
    }

    uint64_t seq = 0;
    while (true) {
        std::vector<std::pair<uint8_t, Frame>> frames_to_send;
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            const bool ready = std::all_of(active_camera_ids.begin(),
                                           active_camera_ids.end(),
                                           [&](uint8_t id) { return has_frame[id]; });
            if (!ready) {
                // 아직 모든 카메라 프레임이 준비되지 않음
            } else {
                frames_to_send.reserve(active_camera_ids.size());
                for (uint8_t id : active_camera_ids) {
                    frames_to_send.emplace_back(id, latest_frames[id]);
                    has_frame[id] = false;
                }
            }
        }

        if (frames_to_send.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        size_t bytes_sent = 0;
        if (!transmitBundle(sock, seq, frames_to_send, bytes_sent)) {
            perror("send");
            break;
        }

        std::cout << "Sent bundle seq=" << seq
                  << " camera_count=" << frames_to_send.size()
                  << " total_bytes=" << bytes_sent << std::endl;
        ++seq;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    for (auto &camera : cameras) {
        camera.device->stop();
    }

    ::close(sock);
    return 0;
}
