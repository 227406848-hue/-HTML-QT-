#include "Trail.h"

#include <cmath>

Trail::Trail(int capacity, float life, float width, float r, float g, float b, float minStep)
    : m_pts(size_t(capacity))
    , m_life(life)
    , m_width(width)
    , m_minStep(minStep)
{
    m_col[0] = r;
    m_col[1] = g;
    m_col[2] = b;
}

void Trail::push(float time, const Vec3 &tip)
{
    const int cap = int(m_pts.size());
    if (m_count > 0) {
        const Point &prev = m_pts[size_t((m_head - 1 + cap) % cap)];
        const float dx = tip.x - prev.p.x;
        const float dy = tip.y - prev.p.y;
        const float dz = tip.z - prev.p.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        // 采样太密就跳过；但已经走远就必须记点，否则高速时会断成虚线
        if (dist < m_minStep && time - prev.t < 0.09f)
            return;
    }

    Point &p = m_pts[size_t(m_head)];
    p.p = tip;
    p.t = time;
    m_head = (m_head + 1) % cap;
    if (m_count < cap)
        ++m_count;
}

void Trail::build(std::vector<float> &out, float now) const
{
    if (m_count < 2)
        return;

    const int cap = int(m_pts.size());
    auto at = [&](int k) -> const Point & {
        const int idx = ((m_head - 1 - k) % cap + cap) % cap;   // k=0 是最新的
        return m_pts[size_t(idx)];
    };

    // 每个顶点 11 个 float：自身位置(3) 相邻点位置(3) 侧向/年龄(2) 颜色(3)
    auto emit = [&](const Point &self, const Point &other, float side, float age) {
        out.push_back(self.p.x);  out.push_back(self.p.y);  out.push_back(self.p.z);
        out.push_back(other.p.x); out.push_back(other.p.y); out.push_back(other.p.z);
        out.push_back(side);
        out.push_back(age);
        out.push_back(m_col[0]);  out.push_back(m_col[1]);  out.push_back(m_col[2]);
    };

    for (int k = 0; k + 1 < m_count; ++k) {
        const Point &a = at(k);        // 较新
        const Point &b = at(k + 1);    // 较旧
        const float ageA = (now - a.t) / m_life;
        const float ageB = (now - b.t) / m_life;
        if (ageA > 1.0f && ageB > 1.0f)
            break;   // 再往后全过期了

        // 世界坐标太近的段直接跳过，免得顶点全挤在一起
        const float dx = a.p.x - b.p.x, dy = a.p.y - b.p.y, dz = a.p.z - b.p.z;
        if (dx * dx + dy * dy + dz * dz < 1e-4f)
            continue;

        // 注意：b 端也要用「a→b」这个方向，否则屏幕空间法线会反向，
        // 两端各往两边撇，四边形就扭成了蝴蝶结（看起来像虚线）。
        // 做法是给 b 端塞一个沿同方向外推的虚拟点。
        const Point ext{{2.0f * b.p.x - a.p.x, 2.0f * b.p.y - a.p.y, 2.0f * b.p.z - a.p.z}, b.t};

        emit(a, b, -1.0f, ageA); emit(a, b, 1.0f, ageA); emit(b, ext, 1.0f, ageB);
        emit(a, b, -1.0f, ageA); emit(b, ext, 1.0f, ageB); emit(b, ext, -1.0f, ageB);
    }
}
