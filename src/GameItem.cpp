#include "GameItem.h"

#include <QCoreApplication>
#include <QFile>

#include <QDebug>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QTimer>
#include <QtConcurrent>

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

// 翼下/翼尖挂架（机体局部坐标，来自原版 PLANE.HARD）
const float kHard[4][3] = {
    { 6.6f, -0.05f, -3.7f},
    {-6.6f, -0.05f, -3.7f},
    { 3.9f, -0.64f, -1.4f},
    {-3.9f, -0.64f, -1.4f},
};

inline float wrapPi(float a)
{
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

inline float damp(float dt, float rate) { return 1.0f - std::exp(-rate * dt); }

struct V3 { float x, y, z; };
inline V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 mul(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline V3 normalize(V3 v)
{
    const float l = std::sqrt(dot(v, v));
    if (l < 1e-9f) return {0, 1, 0};
    return {v.x / l, v.y / l, v.z / l};
}
inline Vec3 toVec3(V3 v) { return {v.x, v.y, v.z}; }
inline Vec3 lerpV(const Vec3 &a, const Vec3 &b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

void lookBasis(V3 dir, float roll, V3 &r, V3 &u, V3 &f)
{
    f = normalize(dir);
    const V3 upRef = (std::fabs(f.y) > 0.995f) ? V3{0, 0, -1} : V3{0, 1, 0};
    r = normalize(cross(f, upRef));
    u = cross(r, f);
    if (roll != 0.0f) {
        const float c = std::cos(roll), s = std::sin(roll);
        const V3 r2 = add(mul(r, c), mul(u, -s));
        const V3 u2 = add(mul(u, c), mul(r, s));
        r = r2; u = u2;
    }
}

} // namespace

GameItem::GameItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_clock.start();

    m_trails.emplace_back(56, 1.25f, 2.2f, 0.94f, 0.96f, 1.00f);
    m_trails.emplace_back(56, 1.25f, 2.2f, 0.94f, 0.96f, 1.00f);
    m_trails.emplace_back(40, 1.60f, 3.4f, 0.98f, 0.92f, 0.82f);
    m_trails.emplace_back(40, 1.60f, 3.4f, 0.98f, 0.92f, 0.82f);

    QtConcurrent::run([this] {
        auto data = world::generate();
        m_world = data;
        m_worldReady.store(true);
        QMetaObject::invokeMethod(this, [this] { emit worldReadyChanged(); },
                                  Qt::QueuedConnection);
        qInfo() << "[world] 生成完成";
    });

    auto *timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(16);
    connect(timer, &QTimer::timeout, this, [this] { update(); });
    timer->start();

    // 遥测给 QML 用（GUI 线程发信号，避免跨线程碰 QML 属性）
    auto *telemetry = new QTimer(this);
    telemetry->setInterval(90);
    connect(telemetry, &QTimer::timeout, this, [this] { emit telemetryChanged(); });
    telemetry->start();

    if (qEnvironmentVariableIntValue("DP_AUTOFIRE") > 0) {
        QTimer::singleShot(1800, this, [this] { fireMissile(); });
        QTimer::singleShot(2400, this, [this] { fireMissile(); });
        QTimer::singleShot(3200, this, [this] { triggerRoll(); });
    }
    // 自检用：DP_LOWALT=1 压到低空看地面投影；DP_SKIM=1 贴着水面飞看浪花
    if (qEnvironmentVariableIntValue("DP_LOWALT") > 0)
        m_flight.setAltitudeForTest(90.0f);
    if (qEnvironmentVariableIntValue("DP_SKIM") > 0) {
        m_flight.setAltitudeForTest(14.0f);
        m_flight.setPosForTest(2600.0f, 2300.0f);   // 挪到海面上
    }

    // 自检用：DP_TACTIC=1 两秒后按一次空格（180° 倒飞）
    if (qEnvironmentVariableIntValue("DP_TACTIC") > 0)
        QTimer::singleShot(2000, this, [this] { spaceTap(); });

    // 自检用：DP_HOLD_BOOST=1 一直按住加力，方便验证燃油消耗
    if (qEnvironmentVariableIntValue("DP_HOLD_BOOST") > 0)
        m_boosting.store(true);

    // 自检用：DP_VIEW=2 直接进座舱视角
    if (qEnvironmentVariableIsSet("DP_VIEW"))
        m_view = m_prevView = qEnvironmentVariableIntValue("DP_VIEW");

    // 音频：截图/自检时用 DP_MUTE=1 静音，平时直接开
    if (qEnvironmentVariableIntValue("DP_MUTE") > 0) {
        m_muted.store(true);
    } else {
        if (!m_audio.start())
            qWarning() << "[audio] waveOut 打开失败，静音跑";

        // 背景音乐：先在 exe 旁边的 assets/music 找，再退回源码目录（开发时直接跑 build/ 里的 exe）
        const QStringList cands = {
            QCoreApplication::applicationDirPath() + QStringLiteral("/assets/music/theme.wav"),
            QStringLiteral(QML_DIR "/../assets/music/theme.wav"),
        };
        bool loaded = false;
        for (const QString &p : cands) {
            if (QFile::exists(p) && m_audio.loadMusic(p.toStdString())) {
                qInfo() << "[audio] 背景音乐" << p;
                loaded = true;
                break;
            }
        }
        if (!loaded)
            qWarning() << "[audio] 没找到背景音乐 assets/music/theme.wav";
    }
}

void GameItem::setStickX(float v)
{
    m_stickX.store(v);
    m_hasStick.store(true);
    emit inputChanged();
}

void GameItem::setStickY(float v)
{
    m_stickY.store(v);
    m_hasStick.store(true);
    emit inputChanged();
}

void GameItem::setBoosting(bool v)
{
    m_boosting.store(v);
    emit inputChanged();
}

void GameItem::triggerRoll()
{
    m_rollRequest.store(true);
}

void GameItem::fireMissile()
{
    m_fireRequest.store(true);
}

void GameItem::spaceTap()
{
    const qint64 now = m_clock.elapsed();

    // 380ms 内的第二击 → 360° 翻滚
    if (m_lastSpaceMs >= 0 && now - m_lastSpaceMs < 380) {
        m_lastSpaceMs = -1;
        m_rollRequest.store(true);
        return;
    }

    m_lastSpaceMs = now;
    const qint64 stamp = now;
    // 等一会儿看有没有第二击；没有才执行 180° 倒飞
    QTimer::singleShot(420, this, [this, stamp] {
        if (m_lastSpaceMs == stamp) {
            m_lastSpaceMs = -1;
            m_flipRequest.store(true);
        }
    });
}

void GameItem::toggleMute()
{
    m_muted.store(!m_muted.load());
    emit inputChanged();
}

void GameItem::cycleView()
{
    m_prevView = m_view;
    m_view = (m_view + 1) % 3;
    m_viewT = 0.0f;
    m_viewOut.store(m_view);
}

void GameItem::stepMissiles(float dt)
{
    const float now = m_clock.elapsed() / 1000.0f;

    // 挂架补弹：打出去 1 秒后重新挂上
    for (int i = 0; i < 4; ++i) {
        if (!m_hard[i].ready && now >= m_hard[i].readyAt)
            m_hard[i].ready = true;
    }

    for (auto &m : m_missiles) {
        if (!m.live)
            continue;
        m.pos = toVec3(add({m.pos.x, m.pos.y, m.pos.z},
                           mul({m.dir.x, m.dir.y, m.dir.z}, 620.0f * dt)));
        m.t += dt;

        const float g = (m_worldReady.load() && m_world)
                            ? world::sampleHeight(*m_world, m.pos.x, m.pos.z)
                            : 0.0f;
        if (m.pos.y <= std::max(g, 0.0f) || m.t > 4.5f) {
            m.live = false;
            m_blastPos[m_blastNext] = m.pos;
            m_blastSpawn[m_blastNext] = now;
            m_blastNext = (m_blastNext + 1) % 6;
            m_particles.burst(m.pos, g < 0.5f);
            m_audio.explosion(m.pos, 1.0f);
        }
    }
}

GameItem::CamView GameItem::viewCam(int view, const Vec3 &plane,
                                    const Vec3 &pr, const Vec3 &pu, const Vec3 &pf) const
{
    CamView c;

    if (view == 0) {
        // 俯视：正上方 205，朝下看，并且跟着机头一起转（否则转向时看不出在往哪走）
        const float yv = m_flight.yaw();
        const float cy = std::cos(yv), sy = std::sin(yv);
        c.pos = {plane.x, plane.y + 205.0f, plane.z};
        c.r = {cy, 0.0f, sy};      // 屏幕向右 = 机体右
        c.u = {sy, 0.0f, -cy};     // 屏幕向上 = 机头方向
        c.f = {0.0f, -1.0f, 0.0f}; // 视线朝下
        c.fov = 0.80f;
        return c;
    }

    if (view == 1) {
        // 追尾：后 84(+16*boost)、上 27，看向机头前方 70
        const float cp = std::cos(m_camPitch), sp = std::sin(m_camPitch);
        const float fx2 = std::sin(m_camYaw) * cp, fy2 = sp, fz2 = -std::cos(m_camYaw) * cp;
        const float back = 84.0f + 16.0f * m_flight.boostLevel();

        float camY = plane.y - fy2 * back + 27.0f;
        if (m_worldReady.load() && m_world) {
            const float g = world::sampleHeight(*m_world, plane.x - fx2 * back,
                                                plane.z - fz2 * back) + 6.0f;
            if (camY < g) camY = g;
        }

        const V3 pos{plane.x - fx2 * back, camY, plane.z - fz2 * back};
        const V3 target{plane.x + fx2 * 70.0f, plane.y + 2.0f, plane.z + fz2 * 70.0f};

        V3 r, u, f;
        lookBasis(sub(target, pos), m_camRoll, r, u, f);
        c.pos = toVec3(pos);
        c.r = toVec3(r);
        c.u = toVec3(u);
        c.f = toVec3(f);
        c.fov = 0.98f + 0.12f * m_flight.boostLevel();
        return c;
    }

    // 座舱：眼点在机头后上方，视线略微下压
    const float s = 1.3f;
    const V3 eye{
        plane.x + pr.x * 0.0f * s + pu.x * 1.72f * s + pf.x * 4.3f * s,
        plane.y + pr.y * 0.0f * s + pu.y * 1.72f * s + pf.y * 4.3f * s,
        plane.z + pr.z * 0.0f * s + pu.z * 1.72f * s + pf.z * 4.3f * s,
    };
    const float ca = std::cos(0.19f), sa = std::sin(0.19f);
    const V3 u{pu.x * ca + pf.x * sa, pu.y * ca + pf.y * sa, pu.z * ca + pf.z * sa};
    const V3 f{pf.x * ca - pu.x * sa, pf.y * ca - pu.y * sa, pf.z * ca - pu.z * sa};

    c.pos = toVec3(eye);
    c.r = pr;
    c.u = toVec3(u);
    c.f = toVec3(f);
    c.fov = 1.16f + 0.08f * m_flight.boostLevel();
    return c;
}

void GameItem::buildFrameState(FrameState &fs)
{
    const qint64 nowNs = m_clock.nsecsElapsed();
    float dt = m_lastNs ? float(double(nowNs - m_lastNs) / 1e9) : 0.016f;
    m_lastNs = nowNs;
    dt = std::clamp(dt, 0.0f, 0.05f);

    InputState in;
    in.stickX = m_stickX.load();
    in.stickY = m_stickY.load();
    in.has = m_hasStick.load();
    in.boost = m_boosting.load();

    // 自检模式：一直压杆贴水面飞，好验证浪花和空气墙
    static const bool skimMode = qEnvironmentVariableIntValue("DP_SKIM") > 0;
    if (skimMode) {
        in.has = true;
        in.stickX = 0.25f;
        in.stickY = -0.55f;
    }

    if (m_rollRequest.exchange(false))
        m_flight.triggerRoll();

    if (m_flipRequest.exchange(false))
        m_flight.toggleInverted();

    const float now = m_clock.elapsed() / 1000.0f;

    float rx, ry, rz, ux, uy, uz, fx, fy, fz;
    m_flight.basis(rx, ry, rz, ux, uy, uz, fx, fy, fz);
    const V3 pr{rx, ry, rz}, pu{ux, uy, uz}, pf{fx, fy, fz};
    const Vec3 plane0 = m_flight.pos();
    const float scale = 1.3f;

    auto hardpointWorld = [&](int i) {
        const float *hp = kHard[i];
        return Vec3{
            plane0.x + pr.x * hp[0] * scale + pu.x * hp[1] * scale + pf.x * hp[2] * scale,
            plane0.y + pr.y * hp[0] * scale + pu.y * hp[1] * scale + pf.y * hp[2] * scale,
            plane0.z + pr.z * hp[0] * scale + pu.z * hp[1] * scale + pf.z * hp[2] * scale,
        };
    };

    // 发射：从挂架位置打出去，瞄前方 900 单位的地面
    if (m_fireRequest.exchange(false)) {
        int slot = -1;
        for (int i = 0; i < 4; ++i) {
            if (m_hard[i].ready) { slot = i; break; }
        }
        for (int i = 0; i < 8 && slot >= 0; ++i) {
            Missile &m = m_missiles[i];
            if (m.live)
                continue;

            m.pos = hardpointWorld(slot);
            const float tx = plane0.x + pf.x * 900.0f;
            const float tz = plane0.z + pf.z * 900.0f;
            const float ty = (m_worldReady.load() && m_world)
                                 ? std::max(world::sampleHeight(*m_world, tx, tz), 0.0f)
                                 : 0.0f;
            const V3 d = normalize({tx - m.pos.x, ty - m.pos.y, tz - m.pos.z});
            m.dir = toVec3(d);
            m.t = 0.0f;
            m.live = true;
            m.slot = slot;

            m_hard[slot].ready = false;
            m_hard[slot].readyAt = now + 1.0f;   // 1 秒后补弹
            break;
        }
    }

    float ground = 0.0f;
    if (m_worldReady.load() && m_world)
        ground = world::sampleHeight(*m_world, plane0.x, plane0.z);

    m_flight.update(dt, in, ground);
    stepMissiles(dt);

    // 相机平滑量
    const float yaw = m_flight.yaw();
    const float pitch = m_flight.pitch();
    const float roll = m_flight.roll();
    m_camYaw += wrapPi(yaw - m_camYaw) * damp(dt, 3.2f);
    m_camPitch += (pitch * 0.62f - 0.22f - m_camPitch) * damp(dt, 2.6f);
    m_camRoll += (roll * 0.28f - m_camRoll) * damp(dt, 3.0f);

    // 姿态与位置（update 之后重新取）
    m_flight.basis(rx, ry, rz, ux, uy, uz, fx, fy, fz);
    const Vec3 plane = m_flight.pos();
    const Vec3 P_r{rx, ry, rz}, P_u{ux, uy, uz}, P_f{fx, fy, fz};

    // 掠水浪花效果已按需求移除，这里只保留一个空框，方便以后想加再加

    // 视角切换：0.9 秒内从旧机位平滑过渡到新机位
    if (m_viewT < 1.0f)
        m_viewT = std::min(1.0f, m_viewT + dt / 0.9f);
    const float e = m_viewT * m_viewT * (3.0f - 2.0f * m_viewT);

    const CamView a = viewCam(m_prevView, plane, P_r, P_u, P_f);
    const CamView b = viewCam(m_view, plane, P_r, P_u, P_f);

    CamView cam;
    cam.pos = lerpV(a.pos, b.pos, e);
    cam.r = lerpV(a.r, b.r, e);
    cam.u = lerpV(a.u, b.u, e);
    cam.fov = a.fov + (b.fov - a.fov) * e;

    // 正交化（平移插值后三个轴不再严格正交）
    V3 r3 = normalize({cam.r.x, cam.r.y, cam.r.z});
    V3 u3{ cam.u.x, cam.u.y, cam.u.z };
    u3 = normalize(sub(u3, mul(r3, dot(u3, r3))));
    const V3 f3 = cross(u3, r3);

    fs.camPos = cam.pos;
    fs.camRight = toVec3(r3);
    fs.camUp = toVec3(u3);
    fs.camFwd = toVec3(f3);
    fs.fov = cam.fov;

    const float t = m_clock.elapsed() / 1000.0f;
    fs.time = t;

    fs.planePos = plane;
    fs.planeFwd = P_f;
    fs.planeRight = P_r;
    fs.planeUp = P_u;
    fs.planeShadowRight = {std::cos(yaw), 0.0f, std::sin(yaw)};
    fs.planeScale = scale;
    fs.planeSpan = std::fabs(std::cos(roll + m_flight.spin()));
    fs.ceiling = 1e5f;

    fs.boostLevel = m_flight.boostLevel();
    fs.boosting = m_boosting.load();
    fs.viewIndex = m_view;

    // 音频线程只要相机朝向（爆炸声像用）和静音状态
    {
        AudioParams ap;
        ap.camPos = fs.camPos;
        ap.camRight = fs.camRight;
        ap.mute = m_muted.load();
        m_audio.setParams(ap);

        static const bool dbgAudio = qEnvironmentVariableIntValue("DP_AUDIO_DEBUG") > 0;
        static float lastLog = -10.0f;
        if (dbgAudio && t - lastLog > 5.0f) {
            lastLog = t;
            qInfo() << "[audio] frames" << m_audio.framesPlayed() << "music" << m_audio.hasMusic();
        }
    }

    // 和原版一样：座舱视角权重过半就不画机体
    const float cockpitW = (m_view == 2 ? e : 0.0f) + (m_prevView == 2 ? 1.0f - e : 0.0f);
    fs.hidePlane = cockpitW > 0.72f;

    // 挂架上的导弹
    for (int i = 0; i < 4; ++i) {
        fs.mounted[i].present = m_hard[i].ready;
        fs.mounted[i].pos = hardpointWorld(i);
    }
    fs.mountedCount = 4;

    // 飞行中的导弹
    fs.missileCount = 0;
    for (const auto &m : m_missiles) {
        if (!m.live)
            continue;
        fs.missiles[fs.missileCount].pos = m.pos;
        fs.missiles[fs.missileCount].dir = m.dir;
        fs.missiles[fs.missileCount].live = true;
        if (++fs.missileCount >= 8)
            break;
    }

    // 尾迹与粒子
    m_particles.update(dt);
    {
        auto tipWorld = [&](float lx, float ly, float lz) {
            return Vec3{
                plane.x + P_r.x * lx * scale + P_u.x * ly * scale + P_f.x * lz * scale,
                plane.y + P_r.y * lx * scale + P_u.y * ly * scale + P_f.y * lz * scale,
                plane.z + P_r.z * lx * scale + P_u.z * ly * scale + P_f.z * lz * scale,
            };
        };

        m_trails[0].push(t, tipWorld(6.6f, -0.02f, -4.7f));
        m_trails[1].push(t, tipWorld(-6.6f, -0.02f, -4.7f));

        if (m_flight.boostLevel() > 0.05f) {
            m_trails[2].push(t, tipWorld(1.3f, -1.2f, -7.5f));
            m_trails[3].push(t, tipWorld(-1.3f, -1.2f, -7.5f));
        }

        m_trailVerts.clear();
        for (const Trail &tr : m_trails)
            tr.build(m_trailVerts, t);

        m_partVerts.clear();
        m_particles.build(m_partVerts);

        fs.trailVerts = &m_trailVerts;
        fs.partVerts = &m_partVerts;
    }

    // 爆炸标记
    for (int i = 0; i < 6; ++i) {
        if (m_blastSpawn[i] < 0.0f) {
            fs.blast[i][3] = -1.0f;
            continue;
        }
        const float age = t - m_blastSpawn[i];
        fs.blast[i][0] = m_blastPos[i].x;
        fs.blast[i][1] = m_blastPos[i].y;
        fs.blast[i][2] = m_blastPos[i].z;
        fs.blast[i][3] = age > 30.0f ? -1.0f : age;
    }

    // 遥测
    m_speedOut.store(m_flight.speed());
    m_altOut.store(m_flight.pos().y);
    m_fuelOut.store(m_flight.fuel());
    m_viewOut.store(m_view);
    {
        const float yawDeg = yaw * 180.0f / 3.14159265f;
        m_headingOut.store(std::fmod(yawDeg + 360.0f, 360.0f));
        m_pitchOut.store(m_flight.pitch() * 180.0f / 3.14159265f);
        m_rollOut.store((m_flight.roll() + (m_flight.inverted() ? 3.14159265f : 0.0f))
                        * 180.0f / 3.14159265f);
        m_invertedOut.store(m_flight.inverted());

        // 垂直速度：航迹速度的垂直分量；雷达高度用离地高度
        const float vs = m_flight.speed() * std::sin(m_flight.pitch());
        m_vsOut.store(vs);
        float keepAgl = 0.0f;
        if (m_worldReady.load() && m_world) {
            const float g2 = world::sampleHeight(*m_world, m_flight.pos().x, m_flight.pos().z);
            keepAgl = std::max(0.0f, m_flight.pos().y - std::max(g2, 0.0f));
        }
        m_aglOut.store(keepAgl);

        // 过载：转弯角速度 × 速度 / g，再叠 1（平飞）
        m_gOut.store(1.0f + std::fabs(m_flight.yawRate()) * m_flight.speed() / 9.81f);

        int ready = 0;
        for (int i = 0; i < 4; ++i) ready += m_hard[i].ready ? 1 : 0;
        m_aamOut.store(ready);
    }
}

QSGNode *GameItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Nearest);
    }

    QQuickWindow *w = window();
    if (!w)
        return node;

    // 只有真的传成功才算上传过；否则（比如首帧 GL 还没初始化）下帧继续重试，
    // 不然一次失败就会永远黑屏
    if (!m_uploaded && m_worldReady.load() && m_world) {
        if (m_renderer.uploadWorld(*m_world))
            m_uploaded = true;
    }

    const qreal dpr = w->effectiveDevicePixelRatio();
    const int pxW = qMax(1, int(width() * dpr));
    const int pxH = qMax(1, int(height() * dpr));

    FrameState fs;
    buildFrameState(fs);
    m_renderer.render(fs, pxW, pxH);

    const QImage frame = m_renderer.takeFrame();
    if (!frame.isNull()) {
        QSGTexture *tex = w->createTextureFromImage(frame, QQuickWindow::TextureIsOpaque);
        node->setTexture(tex);
        node->setRect(QRectF(0, 0, width(), height()));
        node->setSourceRect(QRectF(0, 0, frame.width(), frame.height()));
    }

    return node;
}
