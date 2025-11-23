// client/qt_viewer/ws_client_viewer.h
#pragma once

#include <QObject>
#include <QtWebSockets/QWebSocket>
#include <QImage>
#include "frame_bundle.h"

class WsClientViewer : public QObject
{
    Q_OBJECT
public:
    explicit WsClientViewer(const QUrl& url, QObject* parent = nullptr);

signals:
    void cameraImageUpdated(int cameraId, const QImage& image);

private slots:
    void onConnected();
    void onBinaryMessageReceived(const QByteArray& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    void parseFrameBundle(const QByteArray& message);

    QWebSocket m_webSocket;
    QUrl m_url;
};