#include "Particles.h"

#include <cmath>

namespace {
inline float rnd(unsigned &s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return float(s & 0xFFFFFF) / float(0xFFFFFF);
}
}

void Particles::spawn(const Vec3 &pos, const Vec3 &vel, float life, float size, float kind)
{
    P &p = m_p[m_next];
    m_next = (m_next + 1) % kMax;
    p.pos = pos;
    p.vel = vel;
    p.age = 0.0f;
    p.life = life;
    p.size = size;
    p.kind = kind;
}

void Particles::burst(const Vec3 &pos, bool water)
{
    // 闪光
    spawn(pos, {0, 0, 0}, 0.20f, 46.0f, 4.0f);

    // 火球：往外四散
    for (int i = 0; i < 14; ++i) {
        const float a = rnd(m_seed) * 6.28318f;
        const float up = 0.2f + rnd(m_seed) * 0.9f;
        const float sp = 18.0f + rnd(m_seed) * 34.0f;
        spawn(pos,
              {std::cos(a) * sp, up * sp * 0.8f, std::sin(a) * sp},
              0.45f + rnd(m_seed) * 0.45f,
              28.0f + rnd(m_seed) * 42.0f,
              2.0f);
    }

    // 烟：慢、飘得久
    for (int i = 0; i < 10; ++i) {
        const float a = rnd(m_seed) * 6.28318f;
        const float sp = 4.0f + rnd(m_seed) * 10.0f;
        spawn(pos,
              {std::cos(a) * sp, 6.0f + rnd(m_seed) * 12.0f, std::sin(a) * sp},
              1.8f + rnd(m_seed) * 1.4f,
              40.0f + rnd(m_seed) * 55.0f,
              3.0f);
    }

    if (water) {
        for (int i = 0; i < 14; ++i) {
            const float a = rnd(m_seed) * 6.28318f;
            const float sp = 10.0f + rnd(m_seed) * 20.0f;
            spawn(pos,
                  {std::cos(a) * sp, 26.0f + rnd(m_seed) * 34.0f, std::sin(a) * sp},
                  0.55f + rnd(m_seed) * 0.5f,
                  12.0f + rnd(m_seed) * 14.0f,
                  1.0f);
        }
    }
}

void Particles::spray(const Vec3 &pos, float dx, float dz, float outSpeed,
                       float lift, int count, float size)
{
    // 掠水的浪花：主方向朝外（远离机身），再随机散一点，上抛后受重力落下
    for (int i = 0; i < count; ++i) {
        const float jx = (rnd(m_seed) - 0.5f) * 0.55f;
        const float jz = (rnd(m_seed) - 0.5f) * 0.55f;
        const float sp = outSpeed * (0.65f + rnd(m_seed) * 0.8f);
        const float up = lift * (0.5f + rnd(m_seed) * 1.1f);
        spawn(pos,
              {(dx + jx) * sp, up, (dz + jz) * sp},
              0.30f + rnd(m_seed) * 0.55f,
              size * (0.6f + rnd(m_seed) * 0.9f),
              1.0f);
    }
}

void Particles::update(float dt)
{
    const float drag = std::exp(-1.2f * dt);
    for (P &p : m_p) {
        if (p.age >= p.life)
            continue;
        p.age += dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.pos.z += p.vel.z * dt;
        p.vel.x *= drag;
        p.vel.z *= drag;
        p.vel.y = p.vel.y * drag - 14.0f * dt;
    }
}

void Particles::build(std::vector<float> &out) const
{
    for (const P &p : m_p) {
        if (p.age >= p.life || p.life <= 0.0f)
            continue;
        const float age01 = p.age / p.life;
        out.push_back(p.pos.x); out.push_back(p.pos.y); out.push_back(p.pos.z);
        out.push_back(p.size);
        out.push_back(age01);
        out.push_back(p.kind);
        out.push_back(0.5f);   // seed，shader 里没用到的话留着占位
    }
}
