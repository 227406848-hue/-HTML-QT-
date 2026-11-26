#version 300 es
in vec3 aA; in vec3 aB; in vec2 aS;
uniform mat4 uVP; uniform vec2 uRes; uniform float uWidth, uPS, uMaxW;
out float vAge;
void main(){
  vec4 a = uVP * vec4(aA, 1.0), b = uVP * vec4(aB, 1.0);
  vec2 sa = a.xy / max(a.w, 1e-3) * uRes * 0.5, sb = b.xy / max(b.w, 1e-3) * uRes * 0.5;
  vec2 d = sb - sa; float len = length(d); d = len > 1e-4 ? d / len : vec2(1.0, 0.0);
  float wpx = clamp(uWidth * uPS / max(a.w, 1.0), 1.0, uMaxW) * (1.0 - 0.35 * aS.y);
  vec2 off = vec2(-d.y, d.x) * aS.x * wpx * 0.5;
  gl_Position = vec4(a.xy + off / (uRes * 0.5) * a.w, a.z, a.w);
  vAge = aS.y;
}