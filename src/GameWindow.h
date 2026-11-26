#pragma once

#include <QQuickView>

class GameWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)

public:
    explicit GameWindow(QWindow *parent = nullptr);

    double fps() const { return m_fps; }
    void requestSnapshot(const QString &path, int frames);

signals:
    void fpsChanged();

private:
    void onFrameSwapped();
    void finishSnapshot();

    QString m_snapPath;
    int m_snapFrame = -1;
    qint64 m_frameIndex = 0;
    bool m_snapDone = false;

    qint64 m_lastNs = 0;
    double m_accum = 0.0;
    int m_accumFrames = 0;
    double m_fps = 0.0;
};
