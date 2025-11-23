// server/cam_grabber/main.cpp (Ubuntu)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>

#include "frame_bundle.h"

#pragma pack(push, 1)
struct UdsPacketHeader {
    uint32_t magic; // 0xDEADBEEF
    uint32_t size;  // 뒤에 오는 bundle 크기
};
#pragma pack(pop)

static constexpr uint32_t UDS_MAGIC   = 0xDEADBEEF;
static const char*        SOCKET_PATH = "/tmp/cam_stream.sock";

// 실제로는 카메라 SDK로부터 JPEG/RAW를 받아서 채우면 됨
std::vector<uint8_t> make_dummy_jpeg_payload(int cam_id, uint32_t& w, uint32_t& h) {
    if (cam_id == 0 || cam_id == 1) {
        w = 640; h = 480;
    } else {
        w = 1280; h = 960; // 예시: downscale된 큰 카메라
    }
    std::string s = "DUMMY_JPEG_CAMERA_" + std::to_string(cam_id);
    return std::vector<uint8_t>(s.begin(), s.end());
}

int main() {
    int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        ::close(sock);
        return 1;
    }

    std::cout << "Connected to UNIX socket: " << SOCKET_PATH << std::endl;

    uint64_t seq = 0;
    while (true) {
        FrameBundleHeader fb{};
        fb.magic        = FB_MAGIC;
        fb.version      = 1;
        fb.camera_count = 3;
        fb.seq          = seq++;

        auto now = std::chrono::steady_clock::now().time_since_epoch();
        fb.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        std::vector<uint8_t> bundle;
        bundle.reserve(4096);

        // bundle 헤더
        bundle.insert(bundle.end(),
                      reinterpret_cast<uint8_t*>(&fb),
                      reinterpret_cast<uint8_t*>(&fb) + sizeof(fb));

        // 카메라 3대
        for (int cam = 0; cam < 3; ++cam) {
            uint32_t w = 0, h = 0;
            std::vector<uint8_t> jpg = make_dummy_jpeg_payload(cam, w, h);

            CameraFrameHeader ch{};
            ch.camera_id    = static_cast<uint8_t>(cam);
            ch.pixel_format = 1; // JPEG
            ch.reserved     = 0;
            ch.width        = w;
            ch.height       = h;
            ch.data_size    = static_cast<uint32_t>(jpg.size());

            bundle.insert(bundle.end(),
                          reinterpret_cast<uint8_t*>(&ch),
                          reinterpret_cast<uint8_t*>(&ch) + sizeof(ch));

            bundle.insert(bundle.end(), jpg.begin(), jpg.end());
        }

        UdsPacketHeader uh{};
        uh.magic = UDS_MAGIC;
        uh.size  = static_cast<uint32_t>(bundle.size());

        std::vector<uint8_t> packet;
        packet.reserve(sizeof(uh) + bundle.size());
        packet.insert(packet.end(),
                      reinterpret_cast<uint8_t*>(&uh),
                      reinterpret_cast<uint8_t*>(&uh) + sizeof(uh));
        packet.insert(packet.end(), bundle.begin(), bundle.end());

        ssize_t sent = ::send(sock, packet.data(), packet.size(), 0);
        if (sent < 0) {
            perror("send");
            break;
        }

        std::cout << "Sent bundle seq=" << fb.seq
                  << " total_bytes=" << packet.size() << std::endl;

        usleep(100 * 1000); // 100ms → 10fps
    }

    ::close(sock);
    return 0;
}