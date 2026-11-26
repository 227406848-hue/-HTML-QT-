#version 300 es
in vec3 aPos; in vec3 aNor; in vec4 aCol;
uniform mat4 uVP; uniform mat4 uModel;
out vec3 vN; out vec3 vCol; out float vMat; out vec3 vLocal;
void main(){ vec4 w = uModel * vec4(aPos, 1.0); vN = mat3(uModel) * aNor; vCol = aCol.rgb; vMat = aCol.a; vLocal = aPos; gl_Position = uVP * w; }