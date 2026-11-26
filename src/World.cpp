#include "World.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace world {
namespace {

constexpr float kPi = 3.14159265358979f;

inline uint32_t hash2i(int ix, int iy, uint32_t s)
{
    // 对齐原版的 Math.imul 语义：32 位无符号乘加
    uint32_t h = uint32_t(ix) * 0x27d4eb2du ^ uint32_t(iy) * 0x165667b1u ^ s;
    h = uint32_t(int32_t(h ^ (h >> 15)) * int32_t(0x85ebca6bu));
    h = uint32_t(int32_t(h ^ (h >> 13)) * int32_t(0xc2b2ae35u));
    return h ^ (h >> 16);
}

struct Tables {
    float gx[16], gy[16];
    Tables()
    {
        for (int i = 0; i < 16; ++i) {
            gx[i] = std::cos(float(i) / 16.0f * kPi * 2.0f);
            gy[i] = std::sin(float(i) / 16.0f * kPi * 2.0f);
        }
    }
};
const Tables T;

inline int modP(int v, int p)
{
    const int r = v % p;
    return r < 0 ? r + p : r;
}

// 周期性梯度噪声，x/y 是格点单位
inline float pnoise(float x, float y, int P, uint32_t s)
{
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const float fx = x - float(x0), fy = y - float(y0);
    const int xa = modP(x0, P), ya = modP(y0, P);
    const int xb = (xa + 1 == P) ? 0 : xa + 1;
    const int yb = (ya + 1 == P) ? 0 : ya + 1;

    const float u = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    const float v = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);

    const int g00 = int(hash2i(xa, ya, s) & 15u);
    const int g10 = int(hash2i(xb, ya, s) & 15u);
    const int g01 = int(hash2i(xa, yb, s) & 15u);
    const int g11 = int(hash2i(xb, yb, s) & 15u);

    const float n00 = T.gx[g00] * fx + T.gy[g00] * fy;
    const float n10 = T.gx[g10] * (fx - 1.0f) + T.gy[g10] * fy;
    const float n01 = T.gx[g01] * fx + T.gy[g01] * (fy - 1.0f);
    const float n11 = T.gx[g11] * (fx - 1.0f) + T.gy[g11] * (fy - 1.0f);

    const float a = n00 + (n10 - n00) * u;
    const float b = n01 + (n11 - n01) * u;
    return (a + (b - a) * v) * 1.5f;
}

inline float fbm(float u, float v, int P, int oct, float gain, uint32_t s)
{
    float a = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int o = 0; o < oct; ++o) {
        sum += a * pnoise(u * float(P), v * float(P), P, s + uint32_t(o) * 131u);
        norm += a;
        a *= gain;
        P *= 2;
    }
    return sum / norm;
}

inline float ridged(float u, float v, int P, int oct, uint32_t s)
{
    float a = 1.0f, sum = 0.0f, norm = 0.0f, w = 1.0f;
    for (int o = 0; o < oct; ++o) {
        float n = 1.0f - std::fabs(pnoise(u * float(P), v * float(P), P, s + uint32_t(o) * 71u));
        n *= n;
        n *= w;
        w = std::clamp(n * 1.6f, 0.0f, 1.0f);
        sum += a * n;
        norm += a;
        a *= 0.5f;
        P *= 2;
    }
    return sum / norm;
}

inline float worley(float u, float v, int P, uint32_t s)
{
    const float x = u * float(P), y = v * float(P);
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    float best = 9.0f;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            const int cx = x0 + i, cy = y0 + j;
            const uint32_t h = hash2i(modP(cx, P), modP(cy, P), s);
            const float px = float(cx) + float(h & 1023u) / 1023.0f;
            const float py = float(cy) + float((h >> 10) & 1023u) / 1023.0f;
            const float d = (px - x) * (px - x) + (py - y) * (py - y);
            if (d < best)
                best = d;
        }
    }
    return std::max(0.0f, 1.0f - std::sqrt(best));
}

inline float sstep(float a, float b, float x)
{
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float heightAtUV(float u, float v)
{
    const float wx = fbm(u, v, 3, 3, 0.5f, 11u);
    const float wy = fbm(u, v, 3, 3, 0.5f, 47u);
    const float uu = u + 0.045f * wx, vv = v + 0.045f * wy;

    const float c = fbm(uu, vv, 6, 8, 0.52f, 901u);                  // 大陆、湖泊、岛屿
    const float m = sstep(-0.30f, 0.16f, fbm(uu + 0.37f, vv + 0.11f, 3, 3, 0.5f, 333u)); // 山脉分布
    const float r = ridged(uu, vv, 12, 5, 5150u);

    float h = (c + 0.035f) * 185.0f;
    if (h > 0.0f) {
        const float inland = sstep(4.0f, 55.0f, h);
        h += inland * m * (r * r * 225.0f + r * 48.0f);
        h += inland * (1.0f - m) * r * 32.0f;
        if (h < 9.0f)
            h = 9.0f * std::pow(h / 9.0f, 1.7f);                     // 压平滩涂
    } else {
        h *= 0.9f;
    }
    return h;
}

} // namespace

DataPtr generate()
{
    auto d = std::make_shared<Data>();
    d->height.assign(size_t(N) * N, 0.0f);
    d->biome.assign(size_t(N) * N, 0);
    d->tex.assign(size_t(N) * N * 4, 0);
    d->cloud.assign(size_t(CN) * CN, 0);

    // —— 高度场：按行切带给多线程 ——
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    const int threads = int(std::clamp(hw, 1u, 16u));
    const int rowsPer = (N + threads - 1) / threads;

    std::vector<std::thread> pool;
    pool.reserve(size_t(threads));
    for (int t = 0; t < threads; ++t) {
        const int j0 = t * rowsPer;
        const int j1 = std::min(N, j0 + rowsPer);
        if (j0 >= j1) break;
        pool.emplace_back([d, j0, j1] {
            for (int j = j0; j < j1; ++j) {
                const float v = float(j) / float(N);
                const size_t o = size_t(j) * N;
                for (int i = 0; i < N; ++i) {
                    const float u = float(i) / float(N);
                    d->height[o + size_t(i)] = heightAtUV(u, v);
                    const float q = fbm(u, v, 28, 3, 0.55f, 7007u) * 0.5f + 0.5f;
                    d->biome[o + size_t(i)] = uint8_t(std::clamp<int>(int(std::lround(q * 255.0f)), 0, 255));
                }
            }
        });
    }
    for (auto &th : pool)
        th.join();

    // —— 高度打包 + 低太阳阴影（沿光走的方向扫一遍）——
    const float drop = SUN_TAN * float(CELL);
    const float soft = 7.0f;
    std::vector<float> shLine(size_t(N));

    for (int j = 0; j < N; ++j) {
        const size_t o = size_t(j) * N;
        float sh = -1e9f;
        for (int k = 0; k < 2 * N; ++k) {
            const int i = (k >= N) ? k - N : k;
            const float s = std::max(d->height[o + size_t(i)], 0.0f);
            sh = std::max(sh - drop, s);
            if (k >= N)
                d->tex[(o + size_t(i)) * 4 + 2] =
                    uint8_t(std::clamp<int>(int(std::lround(std::min(1.0f, (sh - s) / soft) * 255.0f)), 0, 255));
        }
    }

    for (size_t k = 0; k < size_t(N) * N; ++k) {
        float q = (d->height[k] - HMIN) / HSPAN;
        q = std::clamp(q, 0.0f, 1.0f);
        const uint32_t v16 = uint32_t(std::lround(q * 65535.0f));
        d->tex[k * 4 + 0] = uint8_t((v16 >> 8) & 0xFF);
        d->tex[k * 4 + 1] = uint8_t(v16 & 0xFF);
        d->tex[k * 4 + 3] = d->biome[k];
    }

    // —— 云图 ——
    for (int j = 0; j < CN; ++j) {
        const float v = float(j) / float(CN);
        for (int i = 0; i < CN; ++i) {
            const float u = float(i) / float(CN);
            const float wx = fbm(u, v, 5, 3, 0.5f, 21u);
            const float wy = fbm(u, v, 5, 3, 0.5f, 22u);
            const float uu = u + 0.012f * wx, vv = v + 0.012f * wy;

            const float cov = sstep(0.40f, 0.66f, fbm(u, v, 3, 4, 0.5f, 4242u) * 0.5f + 0.5f);
            const float big   = worley(uu, vv, 38, 77u);
            const float mid   = worley(uu + 0.013f, vv + 0.031f, 64, 78u);
            const float small = worley(uu + 0.4f, vv + 0.7f, 112, 79u);
            const float lone  = sstep(0.55f, 0.9f, worley(u, v, 11, 80u));

            const float n = (0.16f + 0.84f * std::max(cov, lone * 0.8f)) *
                            (0.62f * big + 0.26f * mid + 0.16f * small);
            d->cloud[size_t(j) * CN + size_t(i)] =
                uint8_t(std::clamp<int>(int(std::lround(n * 255.0f)), 0, 255));
        }
    }

    return d;
}

float sampleHeight(const Data &d, float x, float z)
{
    if (d.height.empty())
        return 0.0f;

    float u = (x * E1X + z * E1Z) / L * float(N) - 0.5f;
    float v = (x * E2X + z * E2Z) / L * float(N) - 0.5f;

    const int i0 = int(std::floor(u)), j0 = int(std::floor(v));
    const float fu = u - float(i0), fv = v - float(j0);

    const int ia = modP(i0, N), ja = modP(j0, N);
    const int ib = (ia + 1 == N) ? 0 : ia + 1;
    const int jb = (ja + 1 == N) ? 0 : ja + 1;

    const float a = d.height[size_t(ja) * N + ia];
    const float b = d.height[size_t(ja) * N + ib];
    const float c = d.height[size_t(jb) * N + ia];
    const float dd = d.height[size_t(jb) * N + ib];

    return a + (b - a) * fu + (c - a) * fv + (a - b - c + dd) * fu * fv;
}

} // namespace world
