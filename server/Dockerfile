# server/Dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 기본 패키지 설치
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Python 라이브러리 (websockets)
RUN pip3 install --no-cache-dir websockets

# 작업 디렉토리
WORKDIR /app

# 전체 레포 복사
# (실제로는 .dockerignore로 build/, .git 등 제외하는 게 좋음)
COPY . .

# C++ cam_grabber 빌드
RUN cmake -B build -S . && \
    cmake --build build --target cam_grabber

# WebSocket 포트
EXPOSE 8765

# 엔트리포인트 스크립트 복사 (실행권한은 따로 설정)
RUN chmod +x server/entrypoint.sh

# 컨테이너 시작 시:
#  - python ws_bridge_server.py 실행
#  - cam_grabber 실행
CMD ["bash", "server/entrypoint.sh"]