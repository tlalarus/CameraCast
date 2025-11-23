# server/cam_grabber/CMakeLists.txt
add_executable(cam_grabber
    main.cpp
)

# 리눅스에서 pthread 필요하면 추가 (std::thread 사용 시)
if(UNIX)
    target_link_libraries(cam_grabber pthread)
endif()