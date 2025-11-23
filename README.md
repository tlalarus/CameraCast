# CameraCast

CameraCast는 **멀티 카메라 프레임을 고속으로 grab → 경량화된 WebSocket으로 송출(cast)** 하는
리얼타임 스트리밍 프레임워크입니다.

## Architecture
```
[C++ Camera Grabber] -> (UDS: /tmp/cam_stream.sock)
|
v
[Python WebSocket Bridge] -> WebSocket clients
|
v
[Qt/C++ Viewer on Windows]
```

- **카메라 캡처(C++)**  
  GenICam / Hikvision / FLIR 등의 고속 카메라 SDK를 사용하여  
  raw 프레임을 grab하고 JPEG로 인코딩한 뒤 FrameBundle 형태로 묶습니다.

- **브릿지(Python asyncio + WebSocket)**  
  C++에서 전송한 FrameBundle을 WebSocket binary로 그대로 중계합니다.

- **클라이언트(Qt)**  
  WebSocket으로 수신한 FrameBundle을 파싱하여  
  카메라별 JPEG 프레임을 실시간으로 표시합니다.

## Features

- Low-latency IPC via UNIX domain socket  
- High FPS camera grabbing (예: 300–400 FPS grab, 15–30 FPS stream)  
- FrameBundle protocol (multi-camera muxed packet format)  
- Cross-platform WebSocket client (Qt6 / Windows)  
- Dockerized server build

---

## 빌드 & 실행 순서 (Native)

### 1. 공통: 리포지토리 클론

```bash
git clone <your-repo-url> CameraCast
cd CameraCast
```

### 2. 서버 빌드 - C++ Grabber 빌드 
```
cmake -B build -S .
cmake --build build --target cam_grabber
```
빌드 후 바이너리 위치
```
build/server/cam_grabber/cam_grabber
```

### 3. 서버 python websocket bridge 실행 
```
cd CameraCast/server/ws_bridge
python3 ws_bridge_server.py
```
* `/tmp/cam_stream.sock` 에서 UNIX domain socket 서버로 대기
* WebSocket 서버: ws://0.0.0.0:8765

### 4. 서버 c++ grabber 실행
다른 터미널에서 
```
cd CameraCast
./build/server/cam_grabber/cam_grabber
```
* C++ 프로세스가 /tmp/cam_stream.sock으로 FrameBundle을 계속 전송
* Python 브리지에서 이를 받아 WebSocket으로 브로드캐스트

### 5. 클라이언트(윈도우) – Qt Viewer 빌드 & 실행

```
cd CameraCast
cmake -B build -S .
cmake --build build --target qt_viewer
```
빌드 후 실행
```
build/client/qt_viewer/qt_viewer(.exe)
```
qt_viewer는 내부에서 ws://<서버 IP>:8765 로 접속하도록 되어 있으며,
3개의 QLabel에 각 카메라 프레임을 표시합니다.
(서버 IP는 코드 또는 설정에서 192.168.x.x 등으로 변경)

---

## 서버 Docker 이미지 빌드 및 실행
서버(c++ grabber + python websocket 브리지)를 하나의 컨테이너로 띄우는 방법입니다.
### 1. Docker 이미지 빌드
레포 루트에서:
```
cd CameraCast
docker build -t cameracast -f server/Dockerfile .
```
* 이미지 이름: cameracast:latest
* Dockerfile은 C++ cam_grabber 빌드 + Python ws_bridge_server.py 실행 환경을 포함합니다.

### 2. 서버 컨테이너 실행
```
docker run --rm -p 8765:8765 cameracast
```
* 컨테이너 내부:
    * /tmp/cam_stream.sock 기준으로 ws_bridge_server.py ↔ cam_grabber 가 UDS로 통신
    * WebSocket 서버: ws://0.0.0.0:8765

* 호스트/외부에서:
    * 클라이언트(Qt viewer)는 ws://<호스트 IP>:8765 로 접속

* 실제 카메라 SDK를 컨테이너 안에서 사용할 경우,

    * --net=host (GigE 카메라 등 네트워크 직접 접근 필요 시)

    * SDK 라이브러리/런타임 설치

    * /dev 또는 특정 디바이스/소켓 마운트

등이 추가로 필요할 수 있습니다. 이는 사용하는 카메라/SDK 환경에 맞춰 별도 설정합니다.

