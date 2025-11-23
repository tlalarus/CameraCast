// common/frame_bundle.h
#pragma once
#include <cstdint>

#pragma pack(push, 1)

struct FrameBundleHeader {
    uint32_t magic;        // 0xCAFEBABE
    uint16_t version;      // 1
    uint16_t camera_count; // 3
    uint64_t seq;          // 증가하는 패킷 번호
    uint64_t timestamp_us; // 서버 기준 타임스탬프 (us)
};

struct CameraFrameHeader {
    uint8_t  camera_id;    // 0, 1, 2
    uint8_t  pixel_format; // 1 = JPEG
    uint16_t reserved;     // 정렬/확장용
    uint32_t width;
    uint32_t height;
    uint32_t data_size;    // 뒤에 오는 JPEG 바이트 수
};

#pragma pack(pop)

static constexpr uint32_t FB_MAGIC = 0xCAFEBABE;