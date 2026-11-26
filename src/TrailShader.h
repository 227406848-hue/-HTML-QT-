#pragma once

// 尾迹 / 粒子着色器：原版 VS/FS_TRAIL、VS/FS_PART 移植到 330 core。
// 改动同地形：版本号、去 precision、输出 alpha 由 0 改 1（QML 纹理不能透明）。

static const char *kVsTrail = R"GLSL(#version 330 core
// 每个顶点带自己的位置 aA 和相邻点位置 aB：屏幕空间里按段方向做垂直偏移。
// （原版所有顶点都锚在 aA 上，靠多次发射拼出带子；这里改成标准带状画法。）
in vec3 aA; in vec3 aB; in vec2 aS; in vec3 aC;
uniform mat4 uVP; uniform vec2 uRes; uniform float uWidth, uPS, uMaxW;
out float vAge; out vec3 vCol;
void main(){
  vec4 a = uVP * vec4(aA, 1.0), b = uVP * vec4(aB, 1.0);
  vec2 sa = a.xy / max(a.w, 1e-3) * uRes * 0.5, sb = b.xy / max(b.w, 1e-3) * uRes * 0.5;
  vec2 d = sb - sa; float len = length(d); d = len > 1e-4 ? d / len : vec2(1.0, 0.0);
  float wpx = clamp(uWidth * uPS / max(a.w, 1.0), 1.0, uMaxW) * (1.0 - 0.35 * aS.y);
  vec2 off = vec2(-d.y, d.x) * aS.x * wpx * 0.5;
  gl_Position = vec4(a.xy + off / (uRes * 0.5) * a.w, a.z, a.w);
  vAge = aS.y; vCol = aC;
})GLSL";

static const char *kFsTrail = R"GLSL(#version 330 core
in float vAge; in vec3 vCol; uniform float uFade0; out vec4 fragColor;
float bayer4(vec2 fc){ ivec2 i = ivec2(fc) & 3;
  const int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(m[i.x + i.y*4]) + 0.5) / 16.0; }
void main(){ float a = 1.0 - smoothstep(uFade0, 1.0, vAge); if (bayer4(gl_FragCoord.xy) > a) discard; fragColor = vec4(vCol, 1.0); })GLSL";

static const char *kVsPart = R"GLSL(#version 330 core
in vec3 aPos; in vec4 aData;   // size, age01, kind, seed
uniform mat4 uVP; uniform vec2 uRes; uniform float uPS;
out vec4 vData; out float vNear;
void main(){ vec4 c = uVP * vec4(aPos, 1.0); gl_Position = c; vData = aData;
  vNear = smoothstep(14.0, 60.0, c.w);
  gl_PointSize = clamp(aData.x * uPS / max(c.w, 1.0), 1.0, 46.0); })GLSL";

static const char *kFsPart = R"GLSL(#version 330 core
in vec4 vData; in float vNear; out vec4 fragColor;
float bayer4(vec2 fc){ ivec2 i = ivec2(fc) & 3;
  const int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(m[i.x + i.y*4]) + 0.5) / 16.0; }
void main(){
  vec2 q = gl_PointCoord * 2.0 - 1.0; float r = dot(q, q); if (r > 1.0) discard;
  float age = vData.y;
  float a = (1.0 - smoothstep(0.5, 1.0, age)) * vNear;
  if (vData.z > 2.5 && vData.z < 3.5) a *= smoothstep(0.03, 0.24, age);        // smoke gathers after the fire
  if (vData.z > 3.5) a = (1.0 - smoothstep(0.35, 1.0, age)) * vNear;
  if (bayer4(gl_FragCoord.xy) > a) discard;
  vec3 c;
  if (vData.z < 0.5) {            // exhaust haze: a spark of heat, then pale ash
    c = age < 0.035 ? vec3(1.0, 0.93, 0.62) : age < 0.075 ? vec3(0.98, 0.66, 0.34) : mix(vec3(0.99, 0.95, 0.88), vec3(0.74, 0.68, 0.78), smoothstep(0.12, 0.9, age));
    if (age >= 0.075 && q.x - q.y > 0.35) c *= vec3(0.80, 0.76, 0.90);     // shaded underside, away from the light
  } else if (vData.z < 1.5) {     // spray
    c = vec3(1.0, 0.97, 0.9);
  } else if (vData.z < 2.5) {     // fireball: white, gold, orange, ember, soot
    c = age < 0.16 ? vec3(1.0, 0.98, 0.86) : age < 0.42 ? vec3(1.0, 0.86, 0.42) : age < 0.66 ? vec3(0.98, 0.56, 0.22) : age < 0.84 ? vec3(0.80, 0.29, 0.20) : vec3(0.40, 0.27, 0.32);
    if (age > 0.16 && age < 0.66 && r < 0.25) c = min(c + vec3(0.02, 0.12, 0.26), 1.0);     // hotter heart
  } else if (vData.z > 3.5) {     // the flash
    c = age < 0.5 ? vec3(1.0, 0.99, 0.93) : vec3(1.0, 0.92, 0.62);
  } else {                        // the dark smoke that hangs afterwards
    c = mix(vec3(0.33, 0.26, 0.34), vec3(0.60, 0.54, 0.64), smoothstep(0.0, 0.9, age));
    if (q.x - q.y > 0.35) c *= vec3(0.80, 0.76, 0.90);
  }
  fragColor = vec4(c, 1.0);
})GLSL";
