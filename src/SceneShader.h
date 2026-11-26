#pragma once

// 从原版 WebGL2 (GLSL ES 300) 移植到桌面 GLSL 330 core。
// 只做了三处改动：版本号、删掉 precision 限定、输出变量改名为 fragColor。
// 地形是屏幕空间光线步进（raymarch）读高度图，逻辑与原版完全一致。
static const char *kSceneFS = R"GLSL(#version 330 core
uniform sampler2D uTerrain, uCloud;
uniform vec3 uCam; uniform mat3 uRot; uniform vec2 uRes; uniform float uTanF, uTime;
uniform vec3 uSunL, uSunV, uSunP;
uniform vec3 uPlane, uPFwd, uPRight; uniform float uPSpan, uPSc;
uniform vec2 uCloudOff; uniform vec2 uNF; uniform float uCeil;
uniform vec4 uBlast[6];   // xyz where a missile landed, w seconds since (negative: unused)
out vec4 fragColor;

const float L = 10240.0, LC = 5120.0, HMIN = -96.0, HSPAN = 512.0, HMAX = 350.0;
const float CB = 215.0, CT = 80.0, C0 = 0.52;
const vec2 E1 = vec2(0.7474, 0.6644), E2 = vec2(-0.6644, 0.7474);
const float FAR = 9000.0;

#define C(r,g,b) (vec3(r,g,b)/255.0)

float hash12(vec2 p){ vec3 p3 = fract(vec3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
  return mix(mix(hash12(i), hash12(i+vec2(1,0)), f.x), mix(hash12(i+vec2(0,1)), hash12(i+vec2(1,1)), f.x), f.y); }
float bayer4(vec2 fc){ ivec2 i = ivec2(fc) & 3;
  const int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(m[i.x + i.y*4]) + 0.5) / 16.0; }

vec2 tuv(vec2 xz){ return vec2(dot(xz,E1), dot(xz,E2)) / L; }
float dec(vec4 s){ return (s.r*65280.0 + s.g*255.0) / 65535.0 * HSPAN + HMIN; }
float hgt(vec2 xz, float lod){ return dec(textureLod(uTerrain, tuv(xz), lod)); }

float cfield(vec2 xz, float lod){ return textureLod(uCloud, vec2(dot(xz,E1), dot(xz,E2)) / LC + uCloudOff, lod).r; }
float cloudTop(float n){ float f = (n - C0) / (1.0 - C0);
  return CB + CT * (f > 0.0 ? sqrt(f) * (0.42 + 0.58*f) : f * 3.0); }

vec3 pick4(vec3 a, vec3 b, vec3 c, vec3 d, float l, float dth){
  float i = clamp(floor(clamp(l, 0.0, 0.999) * 4.0 + (dth - 0.5) * 0.6), 0.0, 3.0);
  return i < 0.5 ? a : i < 1.5 ? b : i < 2.5 ? c : d; }

// ── sky ──
vec3 skyGrad(float y){
  vec3 c = mix(C(255,226,160), C(252,200,134), smoothstep(-0.02, 0.05, y));
  c = mix(c, C(238,164,134), smoothstep(0.04, 0.16, y));
  c = mix(c, C(190,150,172), smoothstep(0.14, 0.32, y));
  c = mix(c, C(124,140,188), smoothstep(0.28, 0.58, y));
  c = mix(c, C(72,104,168), smoothstep(0.5, 1.0, y));
  return c; }
vec3 skyBase(vec3 rd){
  vec3 c = skyGrad(rd.y);
  float az = dot(normalize(rd.xz + 1e-5), normalize(uSunV.xz)) * 0.5 + 0.5;
  float low = 1.0 - smoothstep(0.0, 0.45, rd.y);
  c = mix(c, c * vec3(1.06, 1.03, 0.92) + vec3(0.07, 0.05, 0.0), pow(az, 4.0) * low);
  c = mix(c, mix(c, C(150,140,190), 0.55), pow(1.0 - az, 1.6) * low);
  return c; }

float planeShadow(vec3 p){
  vec3 v = uPlane - p; float s = dot(v, uSunP); if (s < 0.0) return 1.0;
  vec3 q = v - s * uSunP;
  float lx = abs(dot(q, uPRight)) / (max(uPSpan, 0.2) * uPSc), lz = dot(q, uPFwd) / uPSc;
  float body = step(lx, 2.2) * step(-9.6, lz) * step(lz, 4.0) + step(lx, 0.8) * step(4.0, lz) * step(lz, 10.4);
  float wing = step(lx, clamp((2.9 - lz) * 0.95, 0.0, 6.8)) * step(-5.2, lz) * step(lz, 2.9);
  float tail = step(lx, 4.4) * step(-9.4, lz) * step(lz, -6.4);
  return 1.0 - min(body + wing + tail, 1.0) * (1.0 - smoothstep(420.0, 900.0, s)); }

float cloudShadow(vec3 p){
  vec2 q = p.xz + uSunP.xz / uSunP.y * (CB + 22.0 - p.y);
  float n = cfield(q, 0.5);
  return 1.0 - smoothstep(C0 - 0.015, C0 + 0.05, n); }

// what a missile leaves behind: a shock ring, then foam on water or embers and a scorch on land
vec3 blastMark(vec3 col, vec3 p, bool water, float dth){
  for (int i = 0; i < 6; i++) {
    float a = uBlast[i].w; if (a < 0.0) continue;
    float d = length(p.xz - uBlast[i].xz); if (d > 150.0) continue;
    float ring = (1.0 - smoothstep(0.0, 3.4, abs(d - a * 80.0))) * (1.0 - smoothstep(0.3, 1.2, a));
    if (ring > dth) col = mix(col, C(255,246,224), 0.9);
    float n = hash12(floor(p.xz / 2.2) + float(i) * 7.0);
    if (water) {
      float foam = (1.0 - smoothstep(9.0, 22.0, d + n * 7.0)) * (1.0 - smoothstep(0.6, 4.5, a));
      if (foam > dth) col = mix(col, C(255,246,226), 0.85);
    } else {
      float sc = (1.0 - smoothstep(10.0, 22.0, d + n * 8.0)) * (1.0 - smoothstep(16.0, 28.0, a));
      col = mix(col, C(46,32,48), sc * 0.85);
      float ember = (1.0 - smoothstep(3.0, 13.0, d + n * 7.0)) * (1.0 - smoothstep(0.5, 6.0, a));
      if (ember > dth) col = C(255,150,60);
    }
  }
  return col; }

void main(){
  vec2 fc = gl_FragCoord.xy;
  float dth = bayer4(fc);
  vec2 ndc = (fc / uRes) * 2.0 - 1.0;
  vec3 rd = normalize(uRot * vec3(ndc.x * uTanF * uRes.x / uRes.y, ndc.y * uTanF, 1.0));
  vec3 ro = uCam;

  // ── land / water ──
  float tW = rd.y < -1e-5 ? ro.y / -rd.y : 1e9;
  float tmax = min(FAR, tW);
  float t = 0.0, tHit = -1.0;
  bool can = true;
  if (ro.y > HMAX) { if (rd.y >= 0.0) can = false; else t = (ro.y - HMAX) / -rd.y; }
  if (can) {
    float tp = t;
    for (int i = 0; i < 128; i++) {
      if (t > tmax) break;
      vec3 p = ro + rd * t;
      if (p.y > HMAX + 2.0 && rd.y > 0.0) break;
      float lod = clamp(log2(t / 500.0), 0.0, 5.0);
      float d = p.y - hgt(p.xz, lod);
      if (d < 0.0012 * t + 0.02) {
        float a = tp, b = t;
        for (int k = 0; k < 5; k++) { float m = 0.5 * (a + b); vec3 q = ro + rd * m; if (q.y - hgt(q.xz, lod) < 0.0) b = m; else a = m; }
        tHit = b; break;
      }
      tp = t;
      t += max(d * 0.42, 0.35 + t * 0.0045);
    }
  }
  bool water = false;
  if (tHit < 0.0 && tW < FAR) { tHit = tW; water = true; }
  if (tHit > 0.0 && !water && (ro + rd * tHit).y < 0.0) { tHit = tW; water = true; }

  // ── clouds ──
  float tC = -1.0; bool under = false;
  {
    float lim = tHit > 0.0 ? tHit : FAR;
    float ya = CB - 5.0, yb = min(CB + CT + 2.0, uCeil), t0 = 0.0, t1 = lim;
    bool ok = true;
    if (abs(rd.y) < 1e-4) { ok = ro.y > ya && ro.y < yb; }
    else { float a = (ya - ro.y) / rd.y, b = (yb - ro.y) / rd.y; t0 = max(min(a, b), 0.0); t1 = min(max(a, b), lim); }
    if (ok && t1 > t0 && yb > ya) {
      t1 = min(t1, t0 + 5600.0);
      float tt = t0 + 0.3, tp = tt;
      for (int i = 0; i < 64; i++) {
        if (tt > t1) break;
        vec3 p = ro + rd * tt;
        float lod = clamp(log2(tt / 600.0), 0.0, 4.0);
        float top = cloudTop(cfield(p.xz, lod));
        float d = p.y - top;
        if (d < 0.0 && top > CB) {
          float a = tp, b = tt;
          for (int k = 0; k < 5; k++) { float m = 0.5 * (a + b); vec3 q = ro + rd * m; float tq = cloudTop(cfield(q.xz, lod)); if (q.y < tq && tq > CB) b = m; else a = m; }
          tC = b; under = (ro + rd * b).y < CB + 1.5 && rd.y > 0.0; break;
        }
        tp = tt;
        tt += clamp(abs(d) * 0.4, 2.0 + tt * 0.012, 22.0 + tt * 0.03);
      }
    }
  }

  vec3 col; float tOut = FAR * 4.0; float fogT = -1.0;
  vec3 horiz = skyBase(normalize(vec3(rd.x, 0.012, rd.z)));

  if (tC > 0.0) {
    // ── cloud surface ──
    vec3 p = ro + rd * tC; tOut = tC; fogT = tC;
    float lod = clamp(log2(tC / 600.0), 0.0, 4.0), e = 7.0 + tC * 0.004;
    float n0 = cfield(p.xz, lod);
    float tx = cloudTop(cfield(p.xz + vec2(e, 0), lod)) - cloudTop(cfield(p.xz - vec2(e, 0), lod));
    float tz = cloudTop(cfield(p.xz + vec2(0, e), lod)) - cloudTop(cfield(p.xz - vec2(0, e), lod));
    vec3 n = normalize(vec3(-tx, 2.0 * e, -tz));
    float kh = clamp((p.y - CB) / CT, 0.0, 1.0);
    float l = 0.40 + 0.42 * max(dot(n, uSunL), 0.0) + 0.30 * kh;
    l -= step(fract(kh * 5.0 + 0.45), 0.14) * 0.085;               // stacked-contour steps
    if (under) l = 0.12 + 0.1 * n0;
    l *= mix(1.0, planeShadow(p), 0.5);
    col = pick4(C(150,120,168), C(228,168,168), C(255,228,200), C(255,250,238), l, dth);
  } else if (tHit > 0.0) {
    vec3 p = ro + rd * tHit; tOut = tHit; fogT = tHit;
    float lodS = clamp(log2(tHit / 700.0), 0.0, 5.0);
    vec4 s = textureLod(uTerrain, tuv(p.xz), lodS);
    float h = dec(s), bake = s.b, biome = s.a;
    float lit = (1.0 - smoothstep(0.25, 0.75, bake + (dth - 0.5) * 0.3)) * cloudShadow(p);
    float psh = planeShadow(p);
    float fade = smoothstep(2200.0, 500.0, tHit);
    if (water) {
      // ── water ──
      float depth = -h;
      float n1 = vnoise(p.xz * 0.018 + uTime * 0.03), n2 = vnoise(p.xz * 0.09 - uTime * 0.06);
      float d = depth + (n1 - 0.5) * 7.0 + (n2 - 0.5) * 2.5 * fade + (dth - 0.5) * 1.6;
      col = d < 2.6 ? C(150,222,190) : d < 7.5 ? C(72,178,170) : d < 19.0 ? C(38,128,152) : d < 44.0 ? C(30,86,134) : C(24,54,104);
      // sky on the water at low angles
      float fres = pow(1.0 - max(-rd.y, 0.0), 4.0);
      vec3 rf = reflect(rd, normalize(vec3((n2 - 0.5) * 0.10, 1.0, (n1 - 0.5) * 0.10)));
      col = mix(col, skyBase(rf), fres * 0.82);
      // shade
      col = mix(col * vec3(0.56, 0.60, 0.86), col, lit);
      col = mix(col * vec3(0.42, 0.44, 0.66), col, psh);
      // foam along the shore
      float foam = 0.9 + 0.7 * sin(uTime * 1.1 + n1 * 14.0);
      if (depth < foam * fade + 0.4 && hash12(floor(p.xz / 2.0) + floor(uTime * 2.0)) > 0.25) col = mix(C(255,244,220), col, 0.12 + 0.3 * (1.0 - lit));
      // glints: short dashes lying along the wind
      vec2 w = vec2(dot(p.xz, vec2(0.94, 0.34)), dot(p.xz, vec2(-0.34, 0.94))) / vec2(7.0, 2.4);
      vec2 cell = floor(w), fw = fract(w);
      float rnd = hash12(cell);
      float sunPath = pow(max(dot(rf, uSunV), 0.0), 60.0);
      float thr = 0.982 - sunPath * 0.6 + smoothstep(600.0, 2600.0, tHit) * 0.02 * (1.0 - min(sunPath * 4.0, 1.0)) + (1.0 - smoothstep(40.0, 260.0, tHit)) * 0.013;
      float tw = hash12(cell + floor(uTime * 1.7 + rnd * 9.0) * 13.0);
      if (tw > thr && tHit > 40.0 && abs(fw.y - 0.5) < 0.34 && (fw.x > 0.15 || sunPath > 0.2)) col = mix(col, C(255,250,228), lit * 0.75 + 0.25);
      col = blastMark(col, p, true, dth);
    } else {
      // ── land ──
      float e = 5.0 + tHit * 0.004;
      vec3 n = normalize(vec3(hgt(p.xz - vec2(e, 0), lodS) - hgt(p.xz + vec2(e, 0), lodS), 2.0 * e, hgt(p.xz - vec2(0, e), lodS) - hgt(p.xz + vec2(0, e), lodS)));
      vec2 cq = p.xz / 4.4 + (vec2(vnoise(p.xz * 0.085), vnoise(p.xz * 0.085 + 31.7)) - 0.5) * 2.2;   // crowns, nudged off the grid
      vec2 cell = floor(cq), cf = fract(cq);
      float r1 = hash12(cell), r2 = hash12(cell + 91.7);
      float nz = vnoise(p.xz * 0.011), nz2 = vnoise(p.xz * 0.045), nz3 = vnoise(p.xz * 0.0045 + 40.0);
      float slope = 1.0 - n.y;
      float hh = h + (nz2 - 0.5) * 12.0 + (r1 - 0.5) * 3.0 * fade;
      float l = 0.13 + 0.25 * n.y + 0.66 * max(dot(n, uSunL), 0.0) * lit;
      l += (fract(hh / 15.0) - 0.5) * 0.07;                         // terraces
      l *= mix(0.5, 1.0, psh);
      float crown = ((1.0 - cf.x) + (1.0 - cf.y) - 1.0) * 0.30 + (r1 - 0.5) * 0.22;   // lit north-west, dark south-east
      float forest = biome * 0.75 + nz * 0.5;
      if (hh > 224.0 + slope * 70.0) {
        col = pick4(C(124,122,178), C(196,180,208), C(244,222,210), C(255,247,232), l + 0.08, dth);
      } else if (slope > 0.34 || hh > 192.0) {
        l += (r1 - 0.5) * 0.10 * fade - step(fract(hh / 10.0), 0.22) * 0.07;
        col = pick4(C(62,52,88), C(112,96,120), C(166,148,138), C(214,196,168), l, dth);
      } else if (hh < 4.5) {
        col = pick4(C(126,100,112), C(186,154,112), C(228,200,140), C(248,226,170), l + 0.12, dth);
      } else if (forest > 0.52) {
        l += crown * fade;
        float sa = vnoise(p.xz * 0.0052 + 17.0) + (r2 - 0.5) * 0.16, sb = vnoise(p.xz * 0.0105 + 71.0) + (r1 - 0.5) * 0.16;
        if (hh > 104.0 + nz * 46.0 || (nz3 > 0.66 && hh > 22.0) || r2 > 0.955) {
          col = pick4(C(22,42,60), C(30,70,66), C(44,104,78), C(96,152,88), l, dth);
        } else if (sa > 0.60) col = pick4(C(78,58,56), C(170,122,42), C(230,176,48), C(252,216,96), l, dth);
        else if (sb > 0.56)   col = pick4(C(64,30,56), C(132,40,50), C(198,62,56), C(240,124,74), l, dth);
        else                  col = pick4(C(74,34,58), C(156,62,38), C(222,112,40), C(248,168,60), l, dth);
      } else if (hh < 16.0 && biome > 0.47) {
        l += (r1 - 0.5) * 0.08 * fade;
        col = pick4(C(40,66,72), C(78,116,82), C(134,166,88), C(192,208,116), l, dth);    // water meadows
      } else {
        l += (r1 - 0.5) * 0.08 * fade + step(0.5, fract((p.x * 0.6 + p.z * 0.8) / 9.0 + nz * 3.0)) * 0.035 * fade;
        col = pick4(C(92,84,88), C(168,146,84), C(220,194,110), C(246,228,150), l, dth);
      }
      col = blastMark(col, p, false, dth);
    }
  } else {
    // ── sky ──
    col = skyBase(rd);
    // high streaks catching the last light
    if (rd.y > 0.015) {
      vec2 sp = rd.xz / (rd.y + 0.12) * vec2(1.0, 3.2);
      float st = vnoise(sp * 0.9 + vec2(uTime * 0.004, 3.0)) * 0.6 + vnoise(sp * 2.7 + 9.0) * 0.4;
      float cov = smoothstep(0.56, 0.74, st + (dth - 0.5) * 0.08) * smoothstep(0.015, 0.09, rd.y) * (1.0 - smoothstep(0.35, 0.8, rd.y));
      col = mix(col, mix(C(255,214,170), C(232,150,150), smoothstep(0.1, 0.5, rd.y)), cov * 0.8);
    }
    float sd = dot(rd, uSunV);
    col = mix(col, C(255,232,170), smoothstep(0.972, 0.9985, sd) * 0.55);
    if (sd > 0.9965) col = C(255,226,150);
    if (sd > 0.99815) col = C(255,250,226);
  }

  // ── aerial haze ──
  if (fogT > 0.0) {
    float f = 1.0 - exp(-max(fogT - 520.0, 0.0) / 3300.0);
    f = clamp(f + smoothstep(FAR * 0.72, FAR, fogT), 0.0, 1.0);
    f = floor(f * 14.0 + dth) / 14.0;
    col = mix(col, horiz, clamp(f, 0.0, 1.0));
  }
  // posterise to a restrained ramp
  col = floor(col * 28.0 + dth) / 28.0;

  float zc = max(tOut * dot(rd, uRot[2]), uNF.x);
  float zn = (uNF.y + uNF.x) / (uNF.y - uNF.x) - (2.0 * uNF.y * uNF.x) / (uNF.y - uNF.x) / zc;
  gl_FragDepth = clamp(zn * 0.5 + 0.5, 0.0, 1.0);
  // 原版写的是 vec4(col, 0.0)：WebGL 的 canvas 用 alpha:false 创建，
  // 所以 alpha 通道没人管；换成 QML 显示就必须给不透明的 alpha。
  fragColor = vec4(col, 1.0);
})GLSL";
