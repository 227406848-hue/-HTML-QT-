#include "GameWindow.h"

#include <QQuickWindow>
#include <QQmlContext>
#include <QQmlError>
#include <QDebug>
#include <QImage>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QUrl>

GameWindow::GameWindow(QWindow *parent)
    : QQuickView(parent)
{
    setColor(Qt::black);
    setResizeMode(QQuickView::SizeRootObjectToView);

    rootContext()->setContextProperty(QStringLiteral("game"), this);

    setSource(QUrl::fromLocalFile(QStringLiteral(QML_DIR "/Main.qml")));
    if (status() == QQuickView::Error) {
        const auto errs = errors();
        for (const QQmlError &e : errs)
            qWarning() << "[qml]" << e.toString();
    }

    // frameSwapped 每出现一次就是完整一帧（GUI 线程），用来算帧率和触发快照
    connect(this, &QQuickWindow::frameSwapped,
            this, &GameWindow::onFrameSwapped, Qt::QueuedConnection);
}

void GameWindow::requestSnapshot(const QString &path, int frames)
{
    m_snapPath = path;
    m_snapFrame = frames > 0 ? frames : 60;
}

void GameWindow::onFrameSwapped()
{
    static QElapsedTimer clock;
    if (!clock.isValid())
        clock.start();

    const qint64 now = clock.nsecsElapsed();
    if (m_lastNs) {
        const double dt = double(now - m_lastNs) / 1e9;
        m_accum += dt;
        ++m_accumFrames;
        if (m_accum >= 0.5) {
            m_fps = m_accumFrames / m_accum;
            m_accum = 0.0;
            m_accumFrames = 0;
            emit fpsChanged();
        }
    }
    m_lastNs = now;

    ++m_frameIndex;
    if (!m_snapDone && m_snapFrame > 0 && m_frameIndex >= m_snapFrame)
        finishSnapshot();
}

void GameWindow::finishSnapshot()
{
    m_snapDone = true;
    // 现在游戏画面是 QML 里的一个纹理节点，grabWindow 能拿到完整一帧
    const QImage img = grabWindow();
    const bool ok = img.save(m_snapPath);
    qInfo() << "[snapshot]" << (ok ? "saved" : "FAILED") << m_snapPath
            << img.size() << "| fps:" << m_fps;
    QCoreApplication::quit();
}
