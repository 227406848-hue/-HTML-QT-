#pragma once

#include "GameState.h"

// 飞行状态机：直接移植原版 update(dt) 的数学。
// 姿态用 yaw/pitch/roll 三个角描述（原版也是这么做的，四元数只用在相机插值里），
// 转弯速率 omega 带阻尼，roll 跟着 omega 走，速度由加力/俯仰决定。
struct InputState
{
    float stickX = 0.0f;   // -1..1 左右
    float stickY = 0.0f;   // -1..1 前后，正为上仰
    bool  has = false;     // 有没有人握杆
    bool  boost = false;
};

class FlightModel
{
public:
    void reset();
    void update(float dt, const InputState &in, float groundHeight);

    // 花式 360° 翻滚（滚转轴自旋，不影响航向）
    void triggerRoll();
    // 半滚 180°：翻成倒着飞，再按一次翻回来
    void toggleInverted();
    bool inverted() const { return m_flipTo > 0.5f; }
    float spin() const { return m_spin; }

    // 姿态基（right / up / forward），与原版 planeBasis 一致
    void basis(float &rx, float &ry, float &rz,
               float &ux, float &uy, float &uz,
               float &fx, float &fy, float &fz) const;

    Vec3 pos() const { return {px, py, pz}; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    float roll() const { return m_roll; }
    float speed() const { return m_speed; }
    float boostLevel() const { return m_boost; }
    float yawRate() const { return m_omega; }
    bool hitFloor() const { return m_hitFloor; }   // 这一帧是不是顶着空气墙
    float fuel() const { return m_fuel; }   // 0..1，只有座舱 HUD 显示
    void setAltitudeForTest(float y) { py = y; }
    void setPosForTest(float x, float z) { px = x; pz = z; }
    float altitudeAgl() const { return m_agl; }

private:
    float px = 900.0f, py = 400.0f, pz = 1500.0f;
    float m_yaw = -2.2f, m_pitch = 0.0f, m_roll = 0.0f;
    float m_omega = 0.0f;
    float m_speed = 165.0f;
    float m_boost = 0.0f;
    float m_fuel = 1.0f;
    bool m_hitFloor = false;
    float m_agl = 400.0f;
    float m_cruise = 400.0f;

    float m_spinT = -1.0f;   // <0 表示没在翻滚；0..1 是进度
    float m_spin = 0.0f;
    float m_spinDir = 1.0f;

    float m_flipT = 1.0f;    // 半滚动画进度
    float m_flipFrom = 0.0f;
    float m_flipTo = 0.0f;
    float m_flipCur = 0.0f;
};
