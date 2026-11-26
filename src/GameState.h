#pragma once

#include <vector>

// 每帧传给渲染器的状态：相机 + 飞机 + 导弹 + 爆炸标记
struct Vec3
{
    float x = 0, y = 0, z = 0;
};

struct MissileView
{
    Vec3 pos;
    Vec3 dir;
    bool live = false;
};

// 挂在翼下/翼尖挂架上的导弹（还没打出去的）
struct MountedView
{
    Vec3 pos;
    bool present = false;
};

struct FrameState
{
    Vec3 camPos;
    Vec3 camRight{1, 0, 0};
    Vec3 camUp{0, 1, 0};
    Vec3 camFwd{0, 0, -1};
    float fov = 0.8f;

    float time = 0.0f;

    Vec3 planePos;
    Vec3 planeFwd{0, 0, -1};
    Vec3 planeRight{1, 0, 0};
    Vec3 planeUp{0, 1, 0};
    // 地形 shader 画机影用的是水平右向量（不含滚转），和机体基分开传
    Vec3 planeShadowRight{1, 0, 0};
    float planeSpan = 1.0f;
    float planeScale = 1.3f;
    float ceiling = 1e5f;

    bool boosting = false;
    float boostLevel = 0.0f;

    MissileView missiles[8];
    int missileCount = 0;

    MountedView mounted[4];
    int mountedCount = 4;

    // x,y,z = 落点, w = 距爆炸的秒数（<0 表示空槽）
    float blast[6][4] = {};

    // 相机模式：0 俯视 / 1 追尾 / 2 座舱
    int viewIndex = 1;
    // 座舱视角下不画机体，否则眼点在机身里会糊满屏幕
    bool hidePlane = false;

    // 动态几何：尾迹（每顶点 11 float）与粒子（每顶点 7 float）
    const std::vector<float> *trailVerts = nullptr;
    const std::vector<float> *partVerts = nullptr;
};
