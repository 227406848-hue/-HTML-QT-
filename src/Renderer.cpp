#include "Renderer.h"
#include "MeshShader.h"
#include "SceneShader.h"
#include "Shaders.h"
#include "TrailShader.h"
#include "World.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

struct V3 { float x, y, z; };

inline V3 norm(V3 v)
{
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l <= 1e-9f) return {0, 1, 0};
    return {v.x / l, v.y / l, v.z / l};
}

inline V3 crossV(V3 a, V3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// 与原版一致：太阳高度角 27°，可见仰角 8.5°，投影角 50°
struct Sun
{
    V3 L, V, P;
    Sun()
    {
        const float el = std::atan(world::SUN_TAN);
        const float vis = 8.5f * 3.14159265f / 180.0f;
        const float prj = 50.0f * 3.14159265f / 180.0f;
        L = norm({-world::E1X * std::cos(el), std::sin(el), -world::E1Z * std::cos(el)});
        V = norm({-world::E1X * std::cos(vis), std::sin(vis), -world::E1Z * std::cos(vis)});
        P = norm({-world::E1X * std::cos(prj), std::sin(prj), -world::E1Z * std::cos(prj)});
    }
};
const Sun kSun;

// 与原版 lookBasis 相同：从视线方向生成相机/导弹基
void lookBasis(V3 dir, float roll, V3 &r, V3 &u, V3 &f)
{
    f = norm(dir);
    const V3 upRef = (std::fabs(f.y) > 0.995f) ? V3{0, 0, -1} : V3{0, 1, 0};
    r = norm(crossV(f, upRef));
    u = crossV(r, f);
    if (roll != 0.0f) {
        const float c = std::cos(roll), s = std::sin(roll);
        const V3 r2{r.x * c - u.x * s, r.y * c - u.y * s, r.z * c - u.z * s};
        const V3 u2{u.x * c + r.x * s, u.y * c + r.y * s, u.z * c + r.z * s};
        r = r2;
        u = u2;
    }
}

} // namespace

GLuint Renderer::compileProgram(const char *vs, const char *fs, QString *error)
{
    auto build = [&](GLenum type, const char *src) -> GLuint {
        const GLuint shader = glCreateShader(type);
        const GLchar *srcPtr = src;
        glShaderSource(shader, 1, &srcPtr, nullptr);
        glCompileShader(shader);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            QByteArray log(len > 1 ? len : 1, '\0');
            glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
            if (error)
                *error += QStringLiteral("%1 shader: %2\n")
                              .arg(type == GL_VERTEX_SHADER ? QStringLiteral("vertex")
                                                            : QStringLiteral("fragment"),
                                   QString::fromLocal8Bit(log));
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    const GLuint v = build(GL_VERTEX_SHADER, vs);
    const GLuint f = build(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v) glDeleteShader(v);
        if (f) glDeleteShader(f);
        return 0;
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);

    GLint linked = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    glDetachShader(prog, v);
    glDetachShader(prog, f);
    glDeleteShader(v);
    glDeleteShader(f);

    if (!linked) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        QByteArray log(len > 1 ? len : 1, '\0');
        glGetProgramInfoLog(prog, log.size(), nullptr, log.data());
        if (error)
            *error += QStringLiteral("link: %1\n").arg(QString::fromLocal8Bit(log));
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

void Renderer::ensureInit()
{
    if (m_inited)
        return;
    m_inited = true;

    if (!initializeOpenGLFunctions()) {
        qWarning() << "[renderer] OpenGL 3.3 core 函数初始化失败";
        return;
    }

    glGenVertexArrays(1, &m_vao);

    QString err;
    m_progScene = compileProgram(kQuadVS, kSceneFS, &err);
    if (!m_progScene)
        qWarning() << "[renderer] scene program 编译失败:" << err;

    err.clear();
    m_progMesh = compileProgram(kMeshVS, kMeshFS, &err);
    if (!m_progMesh)
        qWarning() << "[renderer] mesh program 编译失败:" << err;

    err.clear();
    m_progTrail = compileProgram(kVsTrail, kFsTrail, &err);
    if (!m_progTrail)
        qWarning() << "[renderer] trail program 编译失败:" << err;

    err.clear();
    m_progPart = compileProgram(kVsPart, kFsPart, &err);
    if (!m_progPart)
        qWarning() << "[renderer] particle program 编译失败:" << err;

    qInfo() << "[renderer] GL" << reinterpret_cast<const char *>(glGetString(GL_VERSION))
            << "|" << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
}

bool Renderer::uploadWorld(const world::Data &data)
{
    // 首帧可能还没初始化 GL（世界生成得比第一帧快时就会碰到），先补上初始化再传
    ensureInit();
    if (!m_inited || data.tex.empty())
        return false;

    const int n = world::N;
    const int cn = world::CN;

    if (!m_terrainTex)
        glGenTextures(1, &m_terrainTex);
    glBindTexture(GL_TEXTURE_2D, m_terrainTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.tex.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    if (!m_cloudTex)
        glGenTextures(1, &m_cloudTex);
    glBindTexture(GL_TEXTURE_2D, m_cloudTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cn, cn, 0, GL_RED, GL_UNSIGNED_BYTE, data.cloud.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_worldReady = true;
    qInfo() << "[renderer] 地形纹理已上传" << n << "x" << n << "| 云图" << cn << "x" << cn;
    return true;
}

void Renderer::ensureTargets(int pxWidth, int pxHeight)
{
    const int lowW = std::max(1, pxWidth / m_scale);
    const int lowH = std::max(1, pxHeight / m_scale);
    if (m_fbo && lowW == m_lowW && lowH == m_lowH)
        return;

    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_tex);
        glDeleteRenderbuffers(1, &m_depthRb);
        m_fbo = 0;
        m_tex = 0;
        m_depthRb = 0;
    }

    m_lowW = lowW;
    m_lowH = lowH;

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_lowW, m_lowH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 深度附件：地形 shader 写 gl_FragDepth，飞机要能被山挡住
    glGenRenderbuffers(1, &m_depthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_lowW, m_lowH);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_depthRb);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        qWarning() << "[renderer] FBO 不完整, status =" << status;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::ensureMesh()
{
    if (m_jetVao)
        return;

    const mesh::Model model = mesh::buildJet();
    m_jetCount = GLsizei(model.body.size());
    m_flameCount = GLsizei(model.flame.size());
    m_mslCount = GLsizei(model.missile.size());

    auto upload = [this](const std::vector<Vertex> &verts, GLuint &vao, GLuint &vbo, GLsizei &count) {
        if (verts.empty()) { vao = 0; count = 0; return; }
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(Vertex)),
                     verts.data(), GL_STATIC_DRAW);
        const GLsizei stride = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void *>(offsetof(Vertex, pos)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void *>(offsetof(Vertex, nor)));
        // aCol 是 vec4：RGB + 紧跟着的材质号
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void *>(offsetof(Vertex, col)));
        count = GLsizei(verts.size());
    };

    upload(model.body, m_jetVao, m_jetVbo, m_jetCount);
    upload(model.flame, m_flameVao, m_flameVbo, m_flameCount);
    upload(model.missile, m_mslVao, m_mslVbo, m_mslCount);
    glBindVertexArray(0);

    qInfo() << "[renderer] 飞机网格" << m_jetCount << "顶点 | 尾焰" << m_flameCount
            << "| 导弹" << m_mslCount;
}

void Renderer::drawMesh(const float *vp, const float *model, float time, float core)
{
    glUniformMatrix4fv(glGetUniformLocation(m_progMesh, "uVP"), 1, GL_FALSE, vp);
    glUniformMatrix4fv(glGetUniformLocation(m_progMesh, "uModel"), 1, GL_FALSE, model);
    glUniform1f(glGetUniformLocation(m_progMesh, "uTime"), time);
    glUniform1f(glGetUniformLocation(m_progMesh, "uCore"), core);
}

void Renderer::render(const FrameState &fs, int pxWidth, int pxHeight)
{
    if (pxWidth <= 0 || pxHeight <= 0)
        return;

    ensureInit();
    ensureTargets(pxWidth, pxHeight);
    if (!m_progScene || !m_worldReady)
        return;

    const float aspect = float(m_lowW) / float(m_lowH);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_lowW, m_lowH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);

    // ── 地形 / 天空 / 云 ──
    glUseProgram(m_progScene);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_terrainTex);
    glUniform1i(glGetUniformLocation(m_progScene, "uTerrain"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_cloudTex);
    glUniform1i(glGetUniformLocation(m_progScene, "uCloud"), 1);

    glUniform3f(glGetUniformLocation(m_progScene, "uCam"), fs.camPos.x, fs.camPos.y, fs.camPos.z);

    const float rot[9] = {
        fs.camRight.x, fs.camRight.y, fs.camRight.z,
        fs.camUp.x,    fs.camUp.y,    fs.camUp.z,
        fs.camFwd.x,   fs.camFwd.y,   fs.camFwd.z
    };
    glUniformMatrix3fv(glGetUniformLocation(m_progScene, "uRot"), 1, GL_FALSE, rot);

    glUniform2f(glGetUniformLocation(m_progScene, "uRes"), float(m_lowW), float(m_lowH));
    glUniform1f(glGetUniformLocation(m_progScene, "uTanF"), std::tan(fs.fov * 0.5f));
    glUniform1f(glGetUniformLocation(m_progScene, "uTime"), fs.time);

    glUniform3f(glGetUniformLocation(m_progScene, "uSunL"), kSun.L.x, kSun.L.y, kSun.L.z);
    glUniform3f(glGetUniformLocation(m_progScene, "uSunV"), kSun.V.x, kSun.V.y, kSun.V.z);
    glUniform3f(glGetUniformLocation(m_progScene, "uSunP"), kSun.P.x, kSun.P.y, kSun.P.z);

    glUniform3f(glGetUniformLocation(m_progScene, "uPlane"), fs.planePos.x, fs.planePos.y, fs.planePos.z);
    // uPFwd 用「反向」：shader 的机影形状把机头摆在 +lz，
    // 但地面上机头的投影相对机身中心落在 -lz 一侧，不取反影子就前后颠倒。
    glUniform3f(glGetUniformLocation(m_progScene, "uPFwd"),
                -fs.planeFwd.x, -fs.planeFwd.y, -fs.planeFwd.z);
    // 注意：机影用的是「水平右向量」，不能拿带滚转的机体右轴，否则影子会歪/反
    glUniform3f(glGetUniformLocation(m_progScene, "uPRight"),
                fs.planeShadowRight.x, fs.planeShadowRight.y, fs.planeShadowRight.z);
    glUniform1f(glGetUniformLocation(m_progScene, "uPSpan"), fs.planeSpan);
    glUniform1f(glGetUniformLocation(m_progScene, "uPSc"), fs.planeScale);

    glUniform2f(glGetUniformLocation(m_progScene, "uCloudOff"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(m_progScene, "uNF"), 0.6f, 40000.0f);
    glUniform1f(glGetUniformLocation(m_progScene, "uCeil"), fs.ceiling);

    float blast[24];
    for (int i = 0; i < 6; ++i)
        for (int k = 0; k < 4; ++k)
            blast[i * 4 + k] = fs.blast[i][k];
    glUniform4fv(glGetUniformLocation(m_progScene, "uBlast"), 6, blast);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);

    // ── 飞机 / 尾焰 / 导弹 ──
    if (m_progMesh) {
        ensureMesh();

        glDepthFunc(GL_LESS);
        glUseProgram(m_progMesh);
        glUniform3f(glGetUniformLocation(m_progMesh, "uSunL"), kSun.L.x, kSun.L.y, kSun.L.z);

        const mesh::Mat4 vp = mesh::Mat4::viewProjection(fs, aspect, 0.6f, 40000.0f);

        const mesh::Mat4 body = mesh::Mat4::model(fs.planeRight, fs.planeUp, fs.planeFwd,
                                                  fs.planePos, fs.planeScale);
        if (!fs.hidePlane) {
            drawMesh(vp.m, body.m, fs.time, 0.0f);
            glBindVertexArray(m_jetVao);
            glDrawArrays(GL_TRIANGLES, 0, m_jetCount);
        }

        // 加力尾焰：内外两层，各画两具发动机
        if (fs.boostLevel > 0.04f && m_flameCount > 0 && !fs.hidePlane) {
            glBindVertexArray(m_flameVao);
            for (int core = 1; core >= 0; --core) {
                drawMesh(vp.m, body.m, fs.time, float(core));
                for (float sgn : {1.0f, -1.0f}) {
                    const float nx = 1.32f * sgn, ny = -0.12f, nz = -9.5f;
                    // 机体局部坐标 → 世界
                    const V3 p{
                        fs.planePos.x + fs.planeRight.x * nx * fs.planeScale + fs.planeUp.x * ny * fs.planeScale + fs.planeFwd.x * nz * fs.planeScale,
                        fs.planePos.y + fs.planeRight.y * nx * fs.planeScale + fs.planeUp.y * ny * fs.planeScale + fs.planeFwd.y * nz * fs.planeScale,
                        fs.planePos.z + fs.planeRight.z * nx * fs.planeScale + fs.planeUp.z * ny * fs.planeScale + fs.planeFwd.z * nz * fs.planeScale,
                    };
                    const float len = (19.0f + 4.0f * std::sin(fs.time * 53.0f + sgn)) * fs.planeScale *
                                      fs.boostLevel * (core ? 0.6f : 1.0f);
                    const float w = (core ? 0.40f : 0.74f) * fs.planeScale *
                                    std::min(1.0f, fs.boostLevel * 2.0f);
                    const mesh::Mat4 fm = mesh::Mat4::modelAniso(fs.planeRight, fs.planeUp, fs.planeFwd,
                                                                 {p.x, p.y, p.z}, w, w, len);
                    drawMesh(vp.m, fm.m, fs.time, float(core));
                    glDrawArrays(GL_TRIANGLES, 0, m_flameCount);
                }
            }
        }

        // 挂架上的导弹（打完 1 秒后重新挂上）
        if (m_mslCount > 0 && !fs.hidePlane) {
            const mesh::Mat4 bodyBasis = mesh::Mat4::model(fs.planeRight, fs.planeUp, fs.planeFwd,
                                                           {0, 0, 0}, fs.planeScale);
            glBindVertexArray(m_mslVao);
            for (int i = 0; i < 4; ++i) {
                if (!fs.mounted[i].present)
                    continue;
                const mesh::Mat4 mm = mesh::Mat4::model(fs.planeRight, fs.planeUp, fs.planeFwd,
                                                        fs.mounted[i].pos, fs.planeScale);
                drawMesh(vp.m, mm.m, fs.time, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, m_mslCount);
            }
            Q_UNUSED(bodyBasis);
        }

        // 飞行中的导弹
        if (m_mslCount > 0) {
            glBindVertexArray(m_mslVao);
            for (int i = 0; i < fs.missileCount; ++i) {
                const MissileView &mi = fs.missiles[i];
                if (!mi.live)
                    continue;
                V3 r, u, f;
                lookBasis({mi.dir.x, mi.dir.y, mi.dir.z}, 0.0f, r, u, f);
                const mesh::Mat4 mm = mesh::Mat4::model({r.x, r.y, r.z}, {u.x, u.y, u.z},
                                                        {f.x, f.y, f.z}, mi.pos,
                                                        fs.planeScale * 1.5f);
                drawMesh(vp.m, mm.m, fs.time, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, m_mslCount);
            }
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ── 尾迹与粒子（最后一个 pass，不进深度写）──
    if (m_progMesh) {
        const mesh::Mat4 vp2 = mesh::Mat4::viewProjection(fs, aspect, 0.6f, 40000.0f);
        drawTrails(fs, vp2.m);
        drawParticles(fs, vp2.m);
    }

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::drawTrails(const FrameState &fs, const float *vp)
{
    if (!m_progTrail || !fs.trailVerts || fs.trailVerts->empty())
        return;

    if (!m_trailVao) {
        glGenVertexArrays(1, &m_trailVao);
        glGenBuffers(1, &m_trailVbo);
    }

    glBindVertexArray(m_trailVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_trailVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(fs.trailVerts->size() * sizeof(float)),
                 fs.trailVerts->data(), GL_DYNAMIC_DRAW);

    const GLsizei stride = 11 * sizeof(float);
    glEnableVertexAttribArray(0);   // aA
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);   // aB
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);   // aS
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);   // aC
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(8 * sizeof(float)));

    glUseProgram(m_progTrail);
    glUniformMatrix4fv(glGetUniformLocation(m_progTrail, "uVP"), 1, GL_FALSE, vp);
    glUniform2f(glGetUniformLocation(m_progTrail, "uRes"), float(m_lowW), float(m_lowH));
    glUniform1f(glGetUniformLocation(m_progTrail, "uWidth"), 5.2f);
    glUniform1f(glGetUniformLocation(m_progTrail, "uPS"), 1.0f);
    glUniform1f(glGetUniformLocation(m_progTrail, "uMaxW"), 34.0f);
    glUniform1f(glGetUniformLocation(m_progTrail, "uFade0"), 0.22f);

    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(fs.trailVerts->size() / 11));
    glDepthMask(GL_TRUE);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::drawParticles(const FrameState &fs, const float *vp)
{
    if (!m_progPart || !fs.partVerts || fs.partVerts->empty())
        return;

    if (!m_partVao) {
        glGenVertexArrays(1, &m_partVao);
        glGenBuffers(1, &m_partVbo);
    }

    glBindVertexArray(m_partVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_partVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(fs.partVerts->size() * sizeof(float)),
                 fs.partVerts->data(), GL_DYNAMIC_DRAW);

    const GLsizei stride = 7 * sizeof(float);
    glEnableVertexAttribArray(0);   // aPos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);   // aData
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));

    glUseProgram(m_progPart);
    glUniformMatrix4fv(glGetUniformLocation(m_progPart, "uVP"), 1, GL_FALSE, vp);
    glUniform2f(glGetUniformLocation(m_progPart, "uRes"), float(m_lowW), float(m_lowH));
    // uPS = 半屏高 / tan(fov/2)：世界尺寸换算成像素的点大小。
    // 原来写死 1.0 会让所有粒子退化成 1 像素的点，水花根本看不见。
    const float pointScale = float(m_lowH) / (2.0f * std::tan(fs.fov * 0.5f));
    glUniform1f(glGetUniformLocation(m_progPart, "uPS"), pointScale);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS, 0, GLsizei(fs.partVerts->size() / 7));
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);

    glBindVertexArray(0);
    glUseProgram(0);
}

QImage Renderer::takeFrame()
{
    if (!m_fbo || m_lowW <= 0 || m_lowH <= 0)
        return {};

    QImage img(m_lowW, m_lowH, QImage::Format_RGBA8888);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_lowW, m_lowH, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return img.flipped(Qt::Vertical);
}
