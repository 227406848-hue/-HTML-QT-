#pragma once

#include <QElapsedTimer>
#include <QQuickItem>

#include <atomic>
#include <memory>
#include <vector>

#include "AudioEngine.h"
#include "FlightModel.h"
#include "GameState.h"
#include "Particles.h"
#include "Renderer.h"
#include "Trail.h"
#include "World.h"

class GameItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool worldReady READ worldReady NOTIFY worldReadyChanged)
    Q_PROPERTY(float stickX READ stickX WRITE setStickX NOTIFY inputChanged)
    Q_PROPERTY(float stickY READ stickY WRITE setStickY NOTIFY inputChanged)
    Q_PROPERTY(bool boosting READ boosting WRITE setBoosting NOTIFY inputChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY inputChanged)

    // 座舱 HUD 用的遥测
    Q_PROPERTY(float speed READ speed NOTIFY telemetryChanged)
    Q_PROPERTY(float altitude READ altitude NOTIFY telemetryChanged)
    Q_PROPERTY(float fuel READ fuel NOTIFY telemetryChanged)
    Q_PROPERTY(int viewIndex READ viewIndex NOTIFY telemetryChanged)
    Q_PROPERTY(float heading READ heading NOTIFY telemetryChanged)
    Q_PROPERTY(float pitchDeg READ pitchDeg NOTIFY telemetryChanged)
    Q_PROPERTY(float rollDeg READ rollDeg NOTIFY telemetryChanged)
    Q_PROPERTY(bool inverted READ inverted NOTIFY telemetryChanged)
    Q_PROPERTY(float verticalSpeed READ verticalSpeed NOTIFY telemetryChanged)
    Q_PROPERTY(float agl READ agl NOTIFY telemetryChanged)
    Q_PROPERTY(float gLoad READ gLoad NOTIFY telemetryChanged)
    Q_PROPERTY(int aam READ aam NOTIFY telemetryChanged)
    Q_PROPERTY(int boostFuel READ boostFuel NOTIFY telemetryChanged)

public:
    explicit GameItem(QQuickItem *parent = nullptr);

    bool worldReady() const { return m_worldReady.load(); }

    float stickX() const { return m_stickX.load(); }
    float stickY() const { return m_stickY.load(); }
    bool boosting() const { return m_boosting.load(); }

    float speed() const { return m_speedOut.load(); }
    float altitude() const { return m_altOut.load(); }
    float fuel() const { return m_fuelOut.load(); }
    int viewIndex() const { return m_viewOut.load(); }
    float heading() const { return m_headingOut.load(); }
    float pitchDeg() const { return m_pitchOut.load(); }
    float rollDeg() const { return m_rollOut.load(); }
    bool inverted() const { return m_invertedOut.load(); }
    float verticalSpeed() const { return m_vsOut.load(); }
    float agl() const { return m_aglOut.load(); }
    float gLoad() const { return m_gOut.load(); }
    int aam() const { return m_aamOut.load(); }
    int boostFuel() const { return int(m_fuelOut.load() * 100.0f); }

    void setStickX(float v);
    void setStickY(float v);
    void setBoosting(bool v);

    Q_INVOKABLE void triggerRoll();
    Q_INVOKABLE void fireMissile();
    Q_INVOKABLE void cycleView();
    Q_INVOKABLE void toggleMute();
    bool muted() const { return m_muted.load(); }
    // 空格：单击 180° 倒飞，双击 360° 翻滚
    Q_INVOKABLE void spaceTap();

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

signals:
    void worldReadyChanged();
    void inputChanged();
    void telemetryChanged();

private:
    struct Missile
    {
        Vec3 pos;
        Vec3 dir;
        float t = 0.0f;
        bool live = false;
        int slot = 0;
    };

    struct Hardpoint
    {
        bool ready = true;
        float readyAt = 0.0f;
    };

    struct CamView
    {
        Vec3 pos, r, u, f;
        float fov = 0.98f;
    };

    CamView viewCam(int view, const Vec3 &plane,
                    const Vec3 &pr, const Vec3 &pu, const Vec3 &pf) const;

    void buildFrameState(FrameState &fs);
    void stepMissiles(float dt);

    Renderer m_renderer;
    world::DataPtr m_world;
    std::atomic<bool> m_worldReady{false};
    bool m_uploaded = false;

    FlightModel m_flight;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;

    std::atomic<float> m_stickX{0.0f};
    std::atomic<float> m_stickY{0.0f};
    std::atomic<bool> m_boosting{false};
    std::atomic<bool> m_hasStick{false};
    std::atomic<bool> m_rollRequest{false};
    std::atomic<bool> m_fireRequest{false};

    std::atomic<float> m_speedOut{0.0f};
    std::atomic<float> m_altOut{0.0f};
    std::atomic<float> m_fuelOut{1.0f};
    std::atomic<int> m_viewOut{1};
    std::atomic<float> m_headingOut{0.0f};
    std::atomic<float> m_pitchOut{0.0f};
    std::atomic<float> m_rollOut{0.0f};
    std::atomic<bool> m_invertedOut{false};
    std::atomic<float> m_vsOut{0.0f};
    std::atomic<float> m_aglOut{0.0f};
    std::atomic<float> m_gOut{1.0f};
    std::atomic<int> m_aamOut{4};

    std::atomic<bool> m_flipRequest{false};
    qint64 m_lastSpaceMs = -1;

    std::vector<Trail> m_trails;
    Particles m_particles;
    std::vector<float> m_trailVerts;
    std::vector<float> m_partVerts;

    Missile m_missiles[8];
    Hardpoint m_hard[4];

    float m_blastSpawn[6] = {-1, -1, -1, -1, -1, -1};
    Vec3 m_blastPos[6];
    int m_blastNext = 0;

    int m_view = 1;
    int m_prevView = 1;
    float m_viewT = 1.0f;

    float m_sprayAcc = 0.0f;  // （水花已停用，留着这个累加器占位）

    AudioEngine m_audio;          // 自己合成的音效
    std::atomic<bool> m_muted{false};
    float m_camYaw = -2.2f;
    float m_camPitch = -0.22f;
    float m_camRoll = 0.0f;
};
