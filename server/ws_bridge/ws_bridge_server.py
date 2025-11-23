# server/ws_bridge/ws_bridge_server.py
import asyncio
import struct
import os
import websockets

SOCKET_PATH = "/tmp/cam_stream.sock"
UDS_MAGIC   = 0xDEADBEEF

ws_clients = set()

async def broadcast_binary(data: bytes):
    if not ws_clients:
        return
    dead = []
    for ws in ws_clients:
        try:
            await ws.send(data)
        except Exception:
            dead.append(ws)
    for ws in dead:
        ws_clients.discard(ws)

async def handle_unix_client(reader: asyncio.StreamReader,
                             writer: asyncio.StreamWriter):
    print("[UNIX] camera process connected")

    UDS_HEADER_FMT  = "<II"
    UDS_HEADER_SIZE = struct.calcsize(UDS_HEADER_FMT)

    try:
        while True:
            header_bytes = await reader.readexactly(UDS_HEADER_SIZE)
            magic, size = struct.unpack(UDS_HEADER_FMT, header_bytes)
            if magic != UDS_MAGIC:
                print(f"[UNIX] invalid magic: {hex(magic)}")
                break

            bundle = await reader.readexactly(size)
            await broadcast_binary(bundle)

    except asyncio.IncompleteReadError:
        print("[UNIX] camera process disconnected (EOF)")
    except Exception as e:
        print(f"[UNIX] error: {e}")
    finally:
        writer.close()
        await writer.wait_closed()
        print("[UNIX] client closed")

async def websocket_handler(websocket, path):
    print("[WS] client connected")
    ws_clients.add(websocket)
    try:
        async for _ in websocket:
            pass
    finally:
        ws_clients.discard(websocket)
        print("[WS] client disconnected")

async def main():
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)

    unix_server = await asyncio.start_unix_server(
        handle_unix_client,
        path=SOCKET_PATH,
    )
    print(f"[UNIX] listening on {SOCKET_PATH}")

    ws_server = await websockets.serve(
        websocket_handler,
        host="0.0.0.0",
        port=8765,
        max_size=None,
        max_queue=None,
    )
    print("[WS] listening on ws://0.0.0.0:8765")

    async with unix_server, ws_server:
        await asyncio.gather(
            unix_server.serve_forever(),
            ws_server.wait_closed(),
        )

if __name__ == "__main__":
    asyncio.run(main())