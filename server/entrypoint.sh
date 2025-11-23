#!/usr/bin/env bash
set -e

# /app 기준 (Dockerfile에서 WORKDIR /app)
cd /app

echo "[ENTRY] starting ws_bridge_server.py ..."
python3 server/ws_bridge/ws_bridge_server.py &

# 잠깐 대기해서 UNIX domain socket 생성되도록 (아주 짧게)
sleep 0.5

echo "[ENTRY] starting cam_grabber ..."
./build/server/cam_grabber/cam_grabber