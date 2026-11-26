#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// 地形/云层数据生成。直接移植自原版 world-src：
// 周期性梯度噪声 + 脊状噪声 + 胞状噪声，生成可无缝平铺的高度场，
// 再烘一张带低太阳阴影的 RGBA 纹理和一张云图。
namespace world {

constexpr int   N        = 2048;              // 高度图每边 texel 数
constexpr int   CELL     = 5;                 // 每个 texel 的世界单位
constexpr float L        = float(N * CELL);   // 10240
constexpr float LC       = L * 0.5f;          // 5120
constexpr float HMIN     = -96.0f;
constexpr float HSPAN    = 512.0f;
constexpr int   CN       = 512;               // 云图每边 texel 数

// 光线水平传播方向（从西北来），x 东 z 南
constexpr float E1X = 0.7474f, E1Z = 0.6644f;
constexpr float E2X = -0.6644f, E2Z = 0.7474f;
constexpr float SUN_TAN = 0.50952545f;        // tan(27°)

struct Data {
    std::vector<float>   height;   // N*N
    std::vector<uint8_t> biome;    // N*N
    std::vector<uint8_t> tex;      // N*N*4  RG=高度 B=烘焙阴影 A=生态
    std::vector<uint8_t> cloud;    // CN*CN
};

using DataPtr = std::shared_ptr<Data>;

// 多线程生成（按行切带），大约 1~2 秒
DataPtr generate();

// 世界坐标下的双线性高度查询（会环绕）
float sampleHeight(const Data &d, float x, float z);

} // namespace world
