// client/qt_viewer/main.cpp
#include <QApplication>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include "ws_client_viewer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QWidget w;
    QHBoxLayout* layout = new QHBoxLayout(&w);

    QLabel* cam0Label = new QLabel("Cam0");
    QLabel* cam1Label = new QLabel("Cam1");
    QLabel* cam2Label = new QLabel("Cam2");

    cam0Label->setMinimumSize(320, 240);
    cam1Label->setMinimumSize(320, 240);
    cam2Label->setMinimumSize(320, 240);

    cam0Label->setScaledContents(true);
    cam1Label->setScaledContents(true);
    cam2Label->setScaledContents(true);

    layout->addWidget(cam0Label);
    layout->addWidget(cam1Label);
    layout->addWidget(cam2Label);

    QUrl url(QStringLiteral("ws://192.168.0.10:8765")); // Ubuntu 서버 IP
    auto* viewer = new WsClientViewer(url, &w);

    QObject::connect(viewer, &WsClientViewer::cameraImageUpdated,
                     [&](int camId, const QImage& img) {
        switch (camId) {
        case 0: cam0Label->setPixmap(QPixmap::fromImage(img)); break;
        case 1: cam1Label->setPixmap(QPixmap::fromImage(img)); break;
        case 2: cam2Label->setPixmap(QPixmap::fromImage(img)); break;
        default: break;
        }
    });

    w.show();
    return a.exec();
}