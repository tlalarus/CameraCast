// client/qt_viewer/ws_client_viewer.cpp
#include "ws_client_viewer.h"
#include <QDebug>

WsClientViewer::WsClientViewer(const QUrl& url, QObject* parent)
    : QObject(parent)
    , m_url(url)
{
    connect(&m_webSocket, &QWebSocket::connected,
            this, &WsClientViewer::onConnected);

    connect(&m_webSocket, &QWebSocket::binaryMessageReceived,
            this, &WsClientViewer::onBinaryMessageReceived);

    connect(&m_webSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &WsClientViewer::onErrorOccurred);

    m_webSocket.open(m_url);
}

void WsClientViewer::onConnected()
{
    qDebug() << "WebSocket connected:" << m_url;
}

void WsClientViewer::onBinaryMessageReceived(const QByteArray& message)
{
    parseFrameBundle(message);
}

void WsClientViewer::onErrorOccurred(QAbstractSocket::SocketError error)
{
    qDebug() << "WebSocket error:" << error << m_webSocket.errorString();
}

void WsClientViewer::parseFrameBundle(const QByteArray& message)
{
    if (message.size() < (int)sizeof(FrameBundleHeader)) {
        qWarning() << "bundle too small";
        return;
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(message.constData());
    const uint8_t* end = ptr + message.size();

    const auto* fb = reinterpret_cast<const FrameBundleHeader*>(ptr);
    if (fb->magic != FB_MAGIC) {
        qWarning() << "invalid bundle magic";
        return;
    }

    if (fb->camera_count == 0 || fb->camera_count > 8) {
        qWarning() << "invalid camera_count" << fb->camera_count;
        return;
    }

    ptr += sizeof(FrameBundleHeader);

    for (uint16_t i = 0; i < fb->camera_count; ++i) {
        if (ptr + sizeof(CameraFrameHeader) > end) {
            qWarning() << "truncated CameraFrameHeader";
            return;
        }

        const auto* ch = reinterpret_cast<const CameraFrameHeader*>(ptr);
        ptr += sizeof(CameraFrameHeader);

        if (ptr + ch->data_size > end) {
            qWarning() << "truncated JPEG data";
            return;
        }

        const uint8_t* jpeg_data = ptr;
        ptr += ch->data_size;

        if (ch->pixel_format != 1) {
            qWarning() << "unsupported pixel_format" << ch->pixel_format;
            continue;
        }

        QByteArray jpegBytes(reinterpret_cast<const char*>(jpeg_data),
                             static_cast<int>(ch->data_size));

        QImage img;
        if (!img.loadFromData(jpegBytes, "JPG")) {
            qWarning() << "failed to decode JPEG for camera" << ch->camera_id;
            continue;
        }

        emit cameraImageUpdated(ch->camera_id, img);
    }
}