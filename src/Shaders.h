#pragma once

// 全屏三角形：用 gl_VertexID 直接算出三个顶点，不需要 VBO。
// 顶点 (0,0) (2,0) (0,2) 覆盖整个 NDC 正方形，uv 在可见区域正好 0..1。
static const char *kQuadVS = R"GLSL(
#version 330 core
out vec2 vUV;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

// 天空渐变：沿用原版 CSS 舞台的五段色带（黄昏，湖乡，最后一缕光）。
static const char *kSkyFS = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 fragColor;

const vec3 C0 = vec3(0.2824, 0.4078, 0.6588); // #4868a8
const vec3 C1 = vec3(0.4863, 0.5490, 0.7373); // #7c8cbc  @ 34%
const vec3 C2 = vec3(0.7451, 0.5882, 0.6745); // #be96ac  @ 58%
const vec3 C3 = vec3(0.9333, 0.6431, 0.5255); // #eea486  @ 78%
const vec3 C4 = vec3(1.0,    0.8863, 0.6275); // #ffe2a0

vec3 ramp(float t)
{
    t = clamp(t, 0.0, 1.0);
    if (t < 0.34) return mix(C0, C1, t / 0.34);
    if (t < 0.58) return mix(C1, C2, (t - 0.34) / 0.24);
    if (t < 0.78) return mix(C2, C3, (t - 0.58) / 0.20);
    return mix(C3, C4, (t - 0.78) / 0.22);
}

void main()
{
    // GL 的 uv.y=0 在屏幕底部，色带定义是从天顶往下，所以这里要翻一下
    fragColor = vec4(ramp(1.0 - vUV.y), 1.0);
}
)GLSL";

// 上采样 pass：低分辨率纹理以 NEAREST 放大，得到像素块。
static const char *kPostFS = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uScene;

void main()
{
    fragColor = texture(uScene, vUV);
}
)GLSL";
