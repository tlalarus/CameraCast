# server/cam_grabber/CMakeLists.txt

add_executable(cam_grabber
    cam_grabber.cpp
    camera_device.h          # header만이라도 명시해두면 IDE 인덱싱에 좋음
    hikvision_camera.cpp
    hikvision_camera.h
    flir_camera.cpp
    flir_camera.h
)

# TODO: 실제 Hikvision / FLIR SDK 라이브러리 링크
# find_library(HIKVISION_LIB MvCameraControl PATHS /opt/MVS/lib ...)
# find_library(FLIR_SPINNAKER_LIB Spinnaker PATHS /opt/spinnaker/lib ...)

# target_link_libraries(cam_grabber
#     ${HIKVISION_LIB}
#     ${FLIR_SPINNAKER_LIB}
#     pthread          # 필요 시
# )
if(UNIX)
    target_link_libraries(cam_grabber pthread)
endif()