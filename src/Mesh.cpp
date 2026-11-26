#include "Mesh.h"

#include <cmath>

namespace mesh {
namespace {

using P3 = std::array<float, 3>;
using ColorFn = std::function<P3(const P3 &normal, const P3 &centroid)>;

constexpr float kPi = 3.14159265358979f;

// 调色板（原版同名常量）
const P3 GREY   {0.74f, 0.78f, 0.84f};
const P3 SLATE  {0.40f, 0.46f, 0.60f};
const P3 PALE   {0.84f, 0.86f, 0.90f};
const P3 RADOME {0.25f, 0.27f, 0.36f};
const P3 DARK   {0.13f, 0.11f, 0.18f};
const P3 NOZZLE {0.34f, 0.29f, 0.33f};
const P3 RED    {0.92f, 0.31f, 0.16f};
const P3 WHITE  {0.95f, 0.93f, 0.88f};
const P3 GLASS  {0.99f, 0.74f, 0.27f};
const P3 GLOW   {1.00f, 0.60f, 0.24f};

ColorFn flat(P3 c)
{
    return [c](const P3 &, const P3 &) { return c; };
}

P3 sub(const P3 &a, const P3 &b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
P3 cross(const P3 &a, const P3 &b)
{
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
float len(const P3 &v) { return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); }

// 双色碎片迷彩 + 浅色机腹
P3 camoColor(const P3 &n, const P3 &c)
{
    if (n[1] < -0.35f)
        return PALE;
    const float s = std::sin(c[2] * 0.62f + c[0] * 0.95f) +
                    std::sin(c[2] * 0.27f - c[0] * 0.7f + 1.9f);
    return s > 0.42f ? SLATE : GREY;
}
const ColorFn camo = camoColor;

struct Builder
{
    std::vector<Vertex> out;

    void push(const P3 &p, const P3 &n, const P3 &c, float mat)
    {
        Vertex v;
        v.pos[0] = p[0]; v.pos[1] = p[1]; v.pos[2] = p[2];
        v.nor[0] = n[0]; v.nor[1] = n[1]; v.nor[2] = n[2];
        v.col[0] = c[0]; v.col[1] = c[1]; v.col[2] = c[2];
        v.mat = mat;
        out.push_back(v);
    }

    void tri(const P3 &a, const P3 &b, const P3 &c, const ColorFn &col, float mat, const P3 &ctr)
    {
        P3 n = cross(sub(b, a), sub(c, a));
        const float l = len(n);
        if (l < 1e-9f)
            return;
        n = {n[0]/l, n[1]/l, n[2]/l};

        const P3 g{(a[0]+b[0]+c[0])/3.0f, (a[1]+b[1]+c[1])/3.0f, (a[2]+b[2]+c[2])/3.0f};
        // 法线朝外：和「三角形重心 - 截面中心」同向
        if (n[0]*(g[0]-ctr[0]) + n[1]*(g[1]-ctr[1]) + n[2]*(g[2]-ctr[2]) < 0.0f)
            n = {-n[0], -n[1], -n[2]};

        const P3 cc = col(n, g);
        push(a, n, cc, mat);
        push(b, n, cc, mat);
        push(c, n, cc, mat);
    }

    // rings 是等长的点环，cols 每个接缝一个颜色；caps 决定首尾是否封口
    void loft(const std::vector<std::vector<P3>> &rings, const std::vector<ColorFn> &cols,
              float mat = 0.0f, bool caps = true)
    {
        if (rings.size() < 2)
            return;
        const size_t K = rings[0].size();

        for (size_t s = 0; s + 1 < rings.size(); ++s) {
            const auto &A = rings[s];
            const auto &B = rings[s + 1];

            P3 ctr{0, 0, 0};
            for (const auto &p : A) { ctr[0] += p[0]; ctr[1] += p[1]; ctr[2] += p[2]; }
            for (const auto &p : B) { ctr[0] += p[0]; ctr[1] += p[1]; ctr[2] += p[2]; }
            ctr[0] /= float(2 * K); ctr[1] /= float(2 * K); ctr[2] /= float(2 * K);

            const ColorFn &col = cols[std::min(s, cols.size() - 1)];

            for (size_t k = 0; k < K; ++k) {
                const size_t k2 = (k + 1) % K;
                tri(A[k], A[k2], B[k2], col, mat, ctr);
                tri(A[k], B[k2], B[k], col, mat, ctr);
            }

            if (caps && (s == 0 || s + 2 == rings.size())) {
                const auto &R = (s == 0) ? A : B;
                P3 c{0, 0, 0};
                for (const auto &p : R) { c[0] += p[0] / float(K); c[1] += p[1] / float(K); c[2] += p[2] / float(K); }
                for (size_t k = 0; k < K; ++k) {
                    const size_t k2 = (k + 1) % K;
                    if (s == 0)
                        tri(c, R[k], R[k2], col, mat, ctr);
                    else
                        tri(c, R[k], R[k2], col, mat, ctr);
                }
            }
        }
    }

    // 一根方截面梁（挂架）
    void beam(const P3 &a, const P3 &b, float w, const ColorFn &col)
    {
        const P3 d = sub(b, a);
        const P3 up = (std::fabs(d[1]) > std::fabs(d[0]) + std::fabs(d[2])) ? P3{1,0,0} : P3{0,1,0};

        P3 s = cross(d, up);
        const float ls = len(s);
        s = {s[0]/ls*w, s[1]/ls*w, s[2]/ls*w};
        P3 t = cross(d, s);
        const float lt = len(t);
        t = {t[0]/lt*w, t[1]/lt*w, t[2]/lt*w};

        auto ring = [&](const P3 &p) {
            std::vector<P3> r(4);
            r[0] = {p[0]+s[0]+t[0], p[1]+s[1]+t[1], p[2]+s[2]+t[2]};
            r[1] = {p[0]-s[0]+t[0], p[1]-s[1]+t[1], p[2]-s[2]+t[2]};
            r[2] = {p[0]-s[0]-t[0], p[1]-s[1]-t[1], p[2]-s[2]-t[2]};
            r[3] = {p[0]+s[0]-t[0], p[1]+s[1]-t[1], p[2]+s[2]-t[2]};
            return r;
        };
        loft({ring(a), ring(b)}, {col});
    }
};

// 椭圆截面环
std::vector<P3> ell(float z, float rx, float ry, float cy, int n = 10, float cx = 0.0f)
{
    // 注意不能写 ring(size_t(n))——那会被解析成函数声明（most vexing parse）
    std::vector<P3> ring(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float a = (float(i) + 0.5f) / float(n) * kPi * 2.0f;
        ring[size_t(i)] = {cx + std::cos(a) * rx, cy + std::sin(a) * ry, z};
    }
    return ring;
}

// 翼型截面（沿 x 展开，点在 z-y 平面内）
std::vector<P3> foil(float x, float c, float zc, float th, float y)
{
    return {
        {x, y,             zc + 0.5f * c},
        {x, y + th,        zc + 0.2f * c},
        {x, y + 0.8f * th, zc - 0.25f * c},
        {x, y + 0.1f * th, zc - 0.5f * c},
        {x, y - 0.3f * th, zc - 0.25f * c},
        {x, y - 0.45f* th, zc + 0.2f * c},
    };
}

// 垂尾截面（在 z-x 平面内）
std::vector<P3> finS(float y, float xc, float c, float zc, float th)
{
    return {
        {xc,            y, zc + 0.5f * c},
        {xc + th,       y, zc + 0.1f * c},
        {xc + th * 0.6f, y, zc - 0.5f * c},
        {xc - th * 0.6f, y, zc - 0.5f * c},
        {xc - th,       y, zc + 0.1f * c},
    };
}

} // namespace

Model buildJet()
{
    Builder b;

    // —— 机身：深色雷达罩 → 迷彩 → 尾锥 ——
    b.loft({
        ell(10.4f, 0.05f, 0.05f, 0.0f),
        ell(9.2f, 0.34f, 0.32f, 0.02f),
        ell(7.6f, 0.62f, 0.58f, 0.08f),
        ell(5.6f, 0.88f, 0.82f, 0.20f),
        ell(3.2f, 1.02f, 0.95f, 0.30f),
        ell(0.5f, 1.05f, 0.90f, 0.30f),
        ell(-3.5f, 1.00f, 0.80f, 0.25f),
        ell(-7.0f, 0.80f, 0.60f, 0.20f),
        ell(-8.9f, 0.32f, 0.28f, 0.20f),
    }, {flat(RADOME), flat(RADOME), camo, camo, camo, camo, camo, camo});

    for (int sgn : {1, -1}) {
        const float s = float(sgn);

        // 进气道：深色进气口 + 长舱段 + 烧灼的喷口
        b.loft({
            ell(2.66f, 0.50f, 0.56f, -0.20f, 8, s * 1.46f),
            ell(2.60f, 0.64f, 0.70f, -0.20f, 8, s * 1.46f),
            ell(1.50f, 0.80f, 0.82f, -0.18f, 8, s * 1.42f),
            ell(-2.00f, 0.90f, 0.86f, -0.14f, 8, s * 1.36f),
            ell(-6.50f, 0.86f, 0.82f, -0.12f, 8, s * 1.32f),
            ell(-8.40f, 0.80f, 0.78f, -0.12f, 8, s * 1.32f),
            ell(-9.50f, 0.60f, 0.60f, -0.12f, 8, s * 1.32f),
        }, {flat(DARK), camo, camo, camo, camo, flat(NOZZLE)});

        // 喷口里的火（材质 4 = 辉光）
        {
            const float cx = s * 1.32f, cy = -0.12f, z = -9.52f, r = 0.5f;
            const int N = 8;
            for (int i = 0; i < N; ++i) {
                const float a = float(i) / float(N) * kPi * 2.0f;
                const float bb = float(i + 1) / float(N) * kPi * 2.0f;
                const P3 p0{cx, cy, z};
                const P3 p1{cx + std::cos(a) * r, cy + std::sin(a) * r, z};
                const P3 p2{cx + std::cos(bb) * r, cy + std::sin(bb) * r, z};
                const P3 n{0, 0, -1};
                // 喷口辉光是个平盘，不参与法线校正，直接塞顶点
                b.push(p0, n, GLOW, 4.0f);
                b.push(p1, n, GLOW, 4.0f);
                b.push(p2, n, GLOW, 4.0f);
            }
        }

        // 边条 / 主翼 / 平尾
        b.loft({foil(s * 0.8f, 5.2f, 4.5f, 0.10f, 0.26f),
                foil(s * 1.75f, 0.5f, 2.5f, 0.08f, 0.14f)}, {camo});

        b.loft({foil(s * 1.7f, 7.6f, -1.3f, 0.26f, 0.05f),
                foil(s * 4.2f, 4.4f, -2.5f, 0.16f, 0.00f),
                foil(s * 6.4f, 2.0f, -3.7f, 0.09f, -0.05f)}, {camo, camo});

        b.loft({foil(s * 1.9f, 3.0f, -7.4f, 0.14f, 0.00f),
                foil(s * 4.3f, 1.2f, -8.5f, 0.07f, -0.02f)}, {camo});

        // 外倾垂尾，顶上一点红
        b.loft({finS(0.55f, s * 1.35f, 3.6f, -6.30f, 0.14f),
                finS(2.80f, s * 2.12f, 2.0f, -7.37f, 0.09f),
                finS(3.50f, s * 2.35f, 1.5f, -7.70f, 0.07f)}, {camo, flat(RED)});

        // 挂架
        b.beam({s * 3.9f, -0.42f, -0.6f}, {s * 3.9f, -0.05f, -1.0f}, 0.09f, flat(SLATE));
        b.beam({s * 3.9f, -0.42f, -2.4f}, {s * 3.9f, -0.05f, -2.6f}, 0.09f, flat(SLATE));
    }

    // 金色泡形座舱盖（材质 3）
    auto can = [](float z, float w, float h, float yb) {
        return std::vector<P3>{
            {-w,        yb - 0.10f, z},
            {-w * 0.86f, yb + 0.72f * h, z},
            {-w * 0.36f, yb + h,        z},
            { w * 0.36f, yb + h,        z},
            { w * 0.86f, yb + 0.72f * h, z},
            { w,        yb - 0.10f, z},
        };
    };
    b.loft({can(6.5f, 0.28f, 0.10f, 0.78f),
            can(5.5f, 0.60f, 0.80f, 0.84f),
            can(4.1f, 0.70f, 0.98f, 0.96f),
            can(2.7f, 0.50f, 0.42f, 1.08f)}, {flat(GLASS)}, 3.0f);

    Model m;
    m.solidEnd = int(b.out.size() / 3);
    m.body = b.out;

    // —— 尾焰：沿 -z 的单位锥，材质 2 ——
    {
        const int FS = 8;
        for (int i = 0; i < FS; ++i) {
            const float a = float(i) / float(FS) * kPi * 2.0f;
            const float bb = float(i + 1) / float(FS) * kPi * 2.0f;
            const P3 p1{std::cos(a), std::sin(a), 0.0f};
            const P3 p2{std::cos(bb), std::sin(bb), 0.0f};
            const P3 tip{0, 0, -1};
            const P3 n{0, 0, 1};
            const P3 white{1, 1, 1};
            for (const P3 &p : {p1, p2, tip}) {
                Vertex v;
                v.pos[0] = p[0]; v.pos[1] = p[1]; v.pos[2] = p[2];
                v.nor[0] = n[0]; v.nor[1] = n[1]; v.nor[2] = n[2];
                v.col[0] = white[0]; v.col[1] = white[1]; v.col[2] = white[2];
                v.mat = 2.0f;
                m.flame.push_back(v);
            }
        }
    }

    // —— 导弹：弹体以自身中点为原点，弹尖 +z ——
    {
        Builder ms;
        ms.loft({
            ell(2.3f, 0.03f, 0.03f, 0.0f, 6),
            ell(1.5f, 0.19f, 0.19f, 0.0f, 6),
            ell(0.9f, 0.19f, 0.19f, 0.0f, 6),
            ell(-1.3f, 0.19f, 0.19f, 0.0f, 6),
            ell(-1.6f, 0.13f, 0.13f, 0.0f, 6),
        }, {flat(RED), flat(RED), flat(WHITE), flat(DARK)});

        // 尾翼：两片十字
        for (int k = 0; k < 2; ++k) {
            const float ax = (k == 0) ? 0.5f : 0.0f;
            const float ay = (k == 0) ? 0.0f : 0.5f;
            std::vector<P3> r1, r2;
            const P3 pts[4] = {{ax, ay, -0.9f}, {ax, ay, -1.55f}, {-ax, -ay, -1.55f}, {-ax, -ay, -0.9f}};
            for (const P3 &q : pts) {
                r1.push_back({q[0] + (ay ? 0.0f : 0.03f), q[1] + (ax ? 0.03f : 0.0f), q[2]});
                r2.push_back({q[0] - (ay ? 0.0f : 0.03f), q[1] - (ax ? 0.03f : 0.0f), q[2]});
            }
            ms.loft({r1, r2}, {flat(SLATE)});
        }
        m.missile = ms.out;
    }

    return m;
}

Mat4 Mat4::viewProjection(const FrameState &fs, float aspect, float nearZ, float farZ)
{
    const float f = 1.0f / std::tan(fs.fov * 0.5f);
    const float bx = -fs.camFwd.x, by = -fs.camFwd.y, bz = -fs.camFwd.z;
    const float *r = &fs.camRight.x;
    const float *u = &fs.camUp.x;
    const float *p = &fs.camPos.x;
    const float b[3] = {bx, by, bz};

    const float V[16] = {
        r[0], u[0], b[0], 0.0f,
        r[1], u[1], b[1], 0.0f,
        r[2], u[2], b[2], 0.0f,
        -(r[0]*p[0] + r[1]*p[1] + r[2]*p[2]),
        -(u[0]*p[0] + u[1]*p[1] + u[2]*p[2]),
        -(b[0]*p[0] + b[1]*p[1] + b[2]*p[2]),
        1.0f
    };

    const float P[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (farZ + nearZ) / (nearZ - farZ), -1.0f,
        0, 0, 2.0f * farZ * nearZ / (nearZ - farZ), 0
    };

    Mat4 out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k)
                s += P[k * 4 + j] * V[i * 4 + k];
            out.m[i * 4 + j] = s;
        }
    return out;
}

Mat4 Mat4::model(const Vec3 &rightv, const Vec3 &upv, const Vec3 &fwdv,
                 const Vec3 &pos, float scale)
{
    return modelAniso(rightv, upv, fwdv, pos, scale, scale, scale);
}

Mat4 Mat4::modelAniso(const Vec3 &rightv, const Vec3 &upv, const Vec3 &fwdv,
                      const Vec3 &pos, float sx, float sy, float sz)
{
    Mat4 out;
    out.m[0] = rightv.x * sx; out.m[1] = rightv.y * sx; out.m[2] = rightv.z * sx; out.m[3] = 0;
    out.m[4] = upv.x * sy;    out.m[5] = upv.y * sy;    out.m[6] = upv.z * sy;    out.m[7] = 0;
    out.m[8] = fwdv.x * sz;   out.m[9] = fwdv.y * sz;   out.m[10] = fwdv.z * sz;  out.m[11] = 0;
    out.m[12] = pos.x;        out.m[13] = pos.y;        out.m[14] = pos.z;        out.m[15] = 1;
    return out;
}

} // namespace mesh
