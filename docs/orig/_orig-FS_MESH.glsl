#version 300 es
precision highp float;
in vec3 vN; in vec3 vCol; in float vMat; in vec3 vLocal;
uniform vec3 uSunL; uniform float uTime, uCore;
out vec4 o;
float bayer4(vec2 fc){ ivec2 i = ivec2(fc) & 3;
  const int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(m[i.x + i.y*4]) + 0.5) / 16.0; }
float hash12(vec2 p){ vec3 p3 = fract(vec3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main(){
  float dth = bayer4(gl_FragCoord.xy);
  if (vMat > 1.5 && vMat < 2.5) {                       // afterburner plume, unit cone along -z
    float k = clamp(-vLocal.z, 0.0, 1.0);
    float flick = hash12(floor(gl_FragCoord.xy * 0.5) + floor(uTime * 30.0));
    if (k > 0.66 + 0.34 * flick) discard;
    if (uCore > 0.5) { o = vec4(k < 0.55 ? vec3(1.0, 0.99, 0.90) : vec3(1.0, 0.88, 0.52), 0.0); return; }
    if (dth > 0.94 - 0.40 * k) discard;                 // stippled sheath, so the white core shows through
    vec3 c = k < 0.20 ? vec3(1.0, 0.90, 0.56) : k < 0.48 ? vec3(1.0, 0.72, 0.30) : k < 0.76 ? vec3(0.97, 0.47, 0.20) : vec3(0.82, 0.26, 0.24);
    if (fract(k * 5.0 + 0.15) < 0.26 && k < 0.8) c = min(c + vec3(0.10, 0.22, 0.30), 1.0);   // shock diamonds
    o = vec4(c, 0.0); return;
  }
  if (vMat > 3.5) { o = vec4(vCol * (0.86 + 0.14 * sin(uTime * 41.0)), 1.0); return; }   // glow in the nozzles
  vec3 n = normalize(vN);
  float l = dot(n, uSunL) * 0.62 + n.y * 0.22 + 0.36;
  float i = floor(clamp(l, 0.0, 0.999) * 3.0 + (dth - 0.5) * 0.35);
  vec3 c = i < 0.5 ? vCol * vec3(0.50, 0.44, 0.66) : i < 1.5 ? vCol * vec3(0.84, 0.78, 0.84) : min(vCol * vec3(1.10, 1.06, 0.98) + vec3(0.03, 0.02, 0.0), 1.0);
  if (vMat > 2.5) c = mix(c, vec3(1.0, 0.97, 0.88), step(0.80, l) * 0.75);   // canopy glint
  o = vec4(c, 1.0);
}