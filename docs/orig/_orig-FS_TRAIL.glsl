#version 300 es
precision highp float;
in float vAge; uniform vec3 uColor; uniform float uFade0; out vec4 o;
float bayer4(vec2 fc){ ivec2 i = ivec2(fc) & 3;
  const int m[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
  return (float(m[i.x + i.y*4]) + 0.5) / 16.0; }
void main(){ float a = 1.0 - smoothstep(uFade0, 1.0, vAge); if (bayer4(gl_FragCoord.xy) > a) discard; o = vec4(uColor, 0.0); }