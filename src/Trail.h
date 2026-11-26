#pragma once

#include <vector>

#include "GameState.h"

// 一条尾迹：环形缓冲存最近若干个采样点，每帧拼成一条面向屏幕的带状三角面。
// 顶点布局（每顶点 11 个 float）：aA(3) aB(3) aS(2: 侧向±1, 年龄) aC(3: 颜色)
class Trail
{
public:
    Trail(int capacity, float life, float width,
          float r, float g, float b, float minStep = 1.6f);

    void push(float time, const Vec3 &tip);
    void build(std::vector<float> &out, float now) const;

private:
    struct Point
    {
        Vec3 p;
        float t = 0.0f;
    };

    std::vector<Point> m_pts;
    int m_head = 0;
    int m_count = 0;
    float m_life;
    float m_width;
    float m_col[3];
    float m_minStep;
    float m_lastT = -1.0f;
};
