#include <QGuiApplication>
#include <QCommandLineParser>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QQmlEngine>

#include "GameWindow.h"
#include "GameItem.h"
#include "AudioEngine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <cstdio>

// GUI 程序在 Windows 上默认看不到 qDebug/qInfo，写一份日志到 exe 旁边
static void fileLogger(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const char *tag = "INFO ";
    switch (type) {
    case QtDebugMsg:    tag = "DEBUG"; break;
    case QtInfoMsg:     tag = "INFO "; break;
    case QtWarningMsg:  tag = "WARN "; break;
    case QtCriticalMsg: tag = "CRIT "; break;
    case QtFatalMsg:    tag = "FATAL"; break;
    }
    const QString line = QStringLiteral("[%1] %2 %3\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                  QString::fromLatin1(tag), msg);
    QFile f(QCoreApplication::applicationDirPath() + QStringLiteral("/dusk-patrol.log"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
    std::fputs(qPrintable(line), stderr);
}

int main(int argc, char *argv[])
{
    // 必须在 QGuiApplication 之前：Qt 用它创建 GL 上下文
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(0);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Dusk Patrol Qt"));
    qInstallMessageHandler(fileLogger);

    // Qt 6 在 Windows 默认 D3D11 RHI，必须强制 OpenGL，
    // 否则 updatePaintNode 里没有 GL 上下文可用
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    qmlRegisterType<GameItem>("DuskPatrol", 1, 0, "GameItem");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Dusk Patrol - Qt/OpenGL rebuild"));
    parser.addHelpOption();
    const QCommandLineOption snapOpt(QStringLiteral("snapshot"),
                                     QStringLiteral("渲染 N 帧后存一张 PNG 并退出."),
                                     QStringLiteral("path"));
    const QCommandLineOption framesOpt(QStringLiteral("frames"),
                                       QStringLiteral("存图前的帧数，默认 60."),
                                       QStringLiteral("n"), QStringLiteral("60"));
    const QCommandLineOption audioOpt(QStringLiteral("audio-render"),
                                      QStringLiteral("离线渲染一段音效试听 WAV 后退出."),
                                      QStringLiteral("path"));
    parser.addOption(snapOpt);
    parser.addOption(framesOpt);
    parser.addOption(audioOpt);
    parser.process(app);

    // 试听渲染：不创建窗口、不碰声卡，直接走同一个混音器
    if (parser.isSet(audioOpt)) {
        AudioEngine engine;
        // 试听也要带背景音乐：按 exe 目录 / 源码目录依次找
        const QString exeDir = QCoreApplication::applicationDirPath();
        const QStringList musicCands = {
            exeDir + QStringLiteral("/assets/music/theme.wav"),
            exeDir + QStringLiteral("/../assets/music/theme.wav"),
            QStringLiteral(QML_DIR "/../assets/music/theme.wav"),
        };
        for (const QString &p : musicCands) {
            if (QFile::exists(p)) {
                engine.loadMusic(p.toStdString());
                break;
            }
        }
        const bool ok = engine.renderDemoWav(qPrintable(parser.value(audioOpt)), 4.5f);
        std::fprintf(stderr, "[audio] render %s\n", ok ? "ok" : "FAILED");
        return ok ? 0 : 1;
    }

    GameWindow window;
    if (parser.isSet(snapOpt))
        window.requestSnapshot(parser.value(snapOpt), parser.value(framesOpt).toInt());

    window.resize(1280, 800);
    window.show();

    return app.exec();
}
