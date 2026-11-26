#include "FlightModel.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

inline float wrapPi(float a)
{
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

// 帧率无关的指数阻尼：每秒衰减到 1/e 的比例
inline float damp(float dt, float rate)
{
    return 1.0f - std::exp(-rate * dt);
}

inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// 死区 + 幂曲线，让摇杆中位附近更细腻（原版 dz）
inline float dz(float v)
{
    const float a = std::fabs(v);
    const float t = clampf((a - 0.04f) / 0.96f, 0.0f, 1.0f);
    return (v < 0 ? -1.0f : 1.0f) * std::pow(t, 1.25f);
}

} // namespace

void FlightModel::triggerRoll()
{
    if (m_spinT >= 0.0f)
        return;                 // 已经在翻了，别叠加
    m_spinT = 0.0f;
    m_spinDir = (m_omega >= 0.0f) ? 1.0f : -1.0f;   // 顺着当前转向翻
}

void FlightModel::toggleInverted()
{
    if (m_flipT < 1.0f)
        return;                     // 正在翻，别叠加
    m_flipFrom = m_flipCur;
    m_flipTo = (m_flipTo > 0.5f) ? 0.0f : kPi;
    m_flipT = 0.0f;
}

void FlightModel::reset()
{
    px = 900.0f; py = 400.0f; pz = 1500.0f;
    m_yaw = -2.2f; m_pitch = 0.0f; m_roll = 0.0f;
    m_omega = 0.0f; m_speed = 165.0f; m_boost = 0.0f; m_agl = 400.0f;
    m_spinT = -1.0f; m_spin = 0.0f; m_fuel = 1.0f;
    m_flipT = 1.0f; m_flipFrom = 0.0f; m_flipTo = 0.0f; m_flipCur = 0.0f;
}

void FlightModel::basis(float &rx, float &ry, float &rz,
                        float &ux, float &uy, float &uz,
                        float &fx, float &fy, float &fz) const
{
    // forward：yaw 绕 y，pitch 抬头为正
    fx = std::sin(m_yaw) * std::cos(m_pitch);
    fy = std::sin(m_pitch);
    fz = -std::cos(m_yaw) * std::cos(m_pitch);

    // 先取水平右向量，再用 roll 绕 forward 旋转
    const float r0x = std::cos(m_yaw), r0y = 0.0f, r0z = std::sin(m_yaw);
    // u0 = cross(r0, f)
    const float u0x = r0y * fz - r0z * fy;
    const float u0y = r0z * fx - r0x * fz;
    const float u0z = r0x * fy - r0y * fx;

    const float totalRoll = m_roll + m_spin + m_flipCur;
    const float c = std::cos(totalRoll), s = std::sin(totalRoll);
    rx = r0x * c + u0x * (-s);
    ry = r0y * c + u0y * (-s);
    rz = r0z * c + u0z * (-s);

    ux = u0x * c + r0x * s;
    uy = u0y * c + r0y * s;
    uz = u0z * c + r0z * s;
}

void FlightModel::update(float dt, const InputState &in, float groundHeight)
{
    if (dt <= 0.0f)
        return;

    // —— 转向 ——
    float wT = 0.0f, pT = 0.0f;
    if (!in.has) {
        // 没人握杆：懒散的 S 型盘旋，并把高度拉回巡航高度
        wT = 0.20f * std::sin(m_speed * 0.0f + 0.6f);
        pT = clampf((m_cruise - py) * 0.0035f, -0.35f, 0.35f);
    } else {
        wT = dz(in.stickX) * 1.85f;
        pT = dz(in.stickY) * 1.05f;
    }

    // —— 前方地面预判：别让山迎面撞上来 ——
    {
        const float cf = std::cos(m_pitch);
        const float fx = std::sin(m_yaw) * cf;
        const float fy = std::sin(m_pitch);
        const float fz = -std::cos(m_yaw) * cf;
        const float taus[3] = {0.5f, 1.1f, 2.0f};
        for (float tau : taus) {
            const float ax = px + fx * m_speed * tau;
            const float az = pz + fz * m_speed * tau;
            const float g = std::max(groundHeight, 0.0f);
            if (g <= 0.5f && tau > 0.6f)
                continue;   // 前方是开阔水面，让她自己贴过去
            const float need = std::atan2(g + (g > 0.5f ? 26.0f : 1.0f) - py,
                                          m_speed * tau);
            if (need > pT)
                pT = std::min(need, 0.9f);
        }
        if (py > 1500.0f)
            pT = std::min(pT, (1700.0f - py) * 0.003f);
    }

    // —— 姿态积分 ——
    m_omega += (wT - m_omega) * damp(dt, 5.6f);
    m_yaw = wrapPi(m_yaw + m_omega * dt);
    m_roll += (m_omega / 1.75f * 1.08f - m_roll) * damp(dt, 5.5f);
    m_pitch += (pT - m_pitch) * damp(dt, 3.8f);

    // 半滚 180°：0.7 秒翻完，smoothstep 收尾
    if (m_flipT < 1.0f) {
        m_flipT = std::min(1.0f, m_flipT + dt / 0.7f);
        const float e = m_flipT * m_flipT * (3.0f - 2.0f * m_flipT);
        m_flipCur = m_flipFrom + (m_flipTo - m_flipFrom) * e;
    }

    // 燃油：满油门 10 秒见底，松开后约 3.5 秒回满；没油就加不了力
    const bool wantBoost = in.boost && m_fuel > 0.0f;
    if (wantBoost)
        m_fuel = std::max(0.0f, m_fuel - dt / 10.0f);
    else
        m_fuel = std::min(1.0f, m_fuel + dt / 3.5f);

    m_boost += ((wantBoost ? 1.0f : 0.0f) - m_boost) * damp(dt, wantBoost ? 5.0f : 2.2f);

    // 花式翻滚：0.9 秒转一整圈，用 smoothstep 收尾
    if (m_spinT >= 0.0f) {
        m_spinT += dt / 0.9f;
        if (m_spinT >= 1.0f) {
            m_spinT = -1.0f;
            m_spin = 0.0f;
        } else {
            const float e = m_spinT;
            m_spin = m_spinDir * kPi * 2.0f * (e * e * (3.0f - 2.0f * e));
        }
    }
    const float vT = (165.0f + (310.0f - 165.0f) * m_boost) - std::sin(m_pitch) * 50.0f;
    m_speed += (vT - m_speed) * damp(dt, 1.5f);

    // —— 位移 ——
    float rx, ry, rz, ux, uy, uz, fx, fy, fz;
    basis(rx, ry, rz, ux, uy, uz, fx, fy, fz);
    px += fx * m_speed * dt;
    py += fy * m_speed * dt;
    pz += fz * m_speed * dt;

    // —— 空气墙：既别穿地，也别插进水里 ——
    // 水面留 9.5：机体半展 8.6，60° 滚转时翼尖下沉 7.4，仍留 2 个单位余量
    const bool overWater = groundHeight < 0.5f;
    // 水下海底高度是负数（深水区能到 -60），水面一律按 y=0 算，
    // 否则 floor 跟着海底一起沉，飞机就真的钻进水里了
    const float base = overWater ? std::max(groundHeight, 0.0f) : groundHeight;
    const float floorY = base + (overWater ? 9.5f : 13.0f);
    if (py < floorY) {
        py = floorY;
        m_hitFloor = true;
        // 撞到空气墙：把下沉的姿态往上推，别老是贴着墙磨
        if (m_pitch < 0.02f)
            m_pitch += (0.02f - m_pitch) * damp(dt, 6.0f);
    } else {
        m_hitFloor = false;
    }
    m_agl = py - (overWater ? std::max(groundHeight, 0.0f) : groundHeight);
}
