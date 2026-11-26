#pragma once

#include <array>
#include <functional>
#include <vector>

#include "GameState.h"

// 飞机网格：照原版 buildPlane 移植（战斗机，带进气道、外倾双垂尾、翼尖挂架）。
// 顶点色 alpha 走材质号：0 蒙皮、2 尾焰、3 座舱玻璃、4 喷口辉光。
struct Vertex
{
    float pos[3];
    float nor[3];
    float col[3];
    float mat;   // 紧跟在 col 后面，着色器把 aCol 当 vec4 读正好拿到它
};

namespace mesh {

struct Model
{
    std::vector<Vertex> body;      // 飞机实体
    std::vector<Vertex> flame;     // 加力尾焰：沿 -z 的单位锥
    std::vector<Vertex> missile;   // 导弹本体（弹尖 +z）
    int solidEnd = 0;
};

Model buildJet();

struct Mat4
{
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    static Mat4 viewProjection(const FrameState &fs, float aspect, float nearZ, float farZ);
    static Mat4 model(const Vec3 &right, const Vec3 &up, const Vec3 &fwd,
                      const Vec3 &pos, float scale);
    static Mat4 modelAniso(const Vec3 &right, const Vec3 &up, const Vec3 &fwd,
                           const Vec3 &pos, float sx, float sy, float sz);
};

} // namespace mesh
