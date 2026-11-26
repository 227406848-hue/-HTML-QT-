#version 300 es
precision highp float;
uniform sampler2D uScene; uniform vec2 uRes;
in vec2 vUV; out vec4 o;
void main(){
  ivec2 p = ivec2(gl_FragCoord.xy);
  vec4 c = texelFetch(uScene, p, 0);
  if (c.a < 0.5) {
    ivec2 m = ivec2(uRes) - 1;
    float n = texelFetch(uScene, clamp(p + ivec2(1,0), ivec2(0), m), 0).a + texelFetch(uScene, clamp(p - ivec2(1,0), ivec2(0), m), 0).a
            + texelFetch(uScene, clamp(p + ivec2(0,1), ivec2(0), m), 0).a + texelFetch(uScene, clamp(p - ivec2(0,1), ivec2(0), m), 0).a;
    if (n > 0.5) c.rgb = vec3(0.105, 0.075, 0.15);
  }
  o = vec4(c.rgb, 1.0);
}