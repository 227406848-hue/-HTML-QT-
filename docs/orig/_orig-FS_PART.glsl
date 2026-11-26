#version 300 es
precision highp float;
in vec4 vData; in float vNear; out vec4 o;
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
  o = vec4(c, 0.0);
}