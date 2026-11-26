#version 300 es
in vec3 aPos; in vec4 aData;   // size, age01, kind, seed
uniform mat4 uVP; uniform vec2 uRes; uniform float uPS;
out vec4 vData; out float vNear;
void main(){ vec4 c = uVP * vec4(aPos, 1.0); gl_Position = c; vData = aData;
  vNear = smoothstep(14.0, 60.0, c.w);
  gl_PointSize = clamp(aData.x * uPS / max(c.w, 1.0), 1.0, 46.0); }