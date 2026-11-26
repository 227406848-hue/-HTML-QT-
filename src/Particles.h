#pragma once

#include <vector>

#include "GameState.h"

// 爆炸/水花粒子：CPU 模拟，GPU 用 GL_POINTS 画（着色器本体照原版 FS_PART）。
// kind: 0 尾气 1 水花 2 火球 3 烟 4 闪光
class Particles
{
public:
    static constexpr int kMax = 640;

    void burst(const Vec3 &pos, bool water);
    // 掠水浪花：往 dir 方向甩出去，同时带一点上抛
    void spray(const Vec3 &pos, float dx, float dz, float outSpeed, float lift, int count, float size);
    void update(float dt);
    void build(std::vector<float> &out) const;   // 每顶点 7 float：pos(3) + data(4)

private:
    void spawn(const Vec3 &pos, const Vec3 &vel, float life, float size, float kind);

    struct P
    {
        Vec3 pos;
        Vec3 vel;
        float age = 1.0f;    // > life 就是死
        float life = 0.0f;
        float size = 0.0f;
        float kind = 0.0f;
    };

    P m_p[kMax];
    int m_next = 0;
    unsigned m_seed = 0x9e3779b9u;
};
