#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QImage>
#include <QString>

#include "GameState.h"
#include "Mesh.h"

namespace world { struct Data; }

// 把游戏画面画进自己的低分辨率 FBO，再交一张 QImage 给 QML 显示。
// （Qt 6 的 RHI 每帧清屏重绘，直接往默认帧缓冲画会被整块盖掉。）
class Renderer : protected QOpenGLFunctions_3_3_Core
{
public:
    // 返回 true 表示真的传上去了（内部会先确保 GL 初始化）
    bool uploadWorld(const world::Data &data);
    void render(const FrameState &fs, int pxWidth, int pxHeight);
    QImage takeFrame();

    bool worldUploaded() const { return m_worldReady; }

private:
    void ensureInit();
    void ensureTargets(int pxWidth, int pxHeight);
    void ensureMesh();
    GLuint compileProgram(const char *vs, const char *fs, QString *error);

    void drawMesh(const float *vp, const float *model, float time, float core);

    bool m_inited = false;
    int m_scale = 4;

    GLuint m_progScene = 0;
    GLuint m_progMesh = 0;
    GLuint m_progTrail = 0;
    GLuint m_progPart = 0;
    GLuint m_vao = 0;
    GLuint m_fbo = 0;
    GLuint m_tex = 0;
    GLuint m_depthRb = 0;
    int m_lowW = 0;
    int m_lowH = 0;

    GLuint m_terrainTex = 0;
    GLuint m_cloudTex = 0;
    bool m_worldReady = false;

    // 三套网格缓冲：机体 / 尾焰 / 导弹
    void drawTrails(const FrameState &fs, const float *vp);
    void drawParticles(const FrameState &fs, const float *vp);

    GLuint m_trailVao = 0, m_trailVbo = 0;
    GLuint m_partVao = 0, m_partVbo = 0;

    GLuint m_jetVao = 0, m_jetVbo = 0;
    GLuint m_flameVao = 0, m_flameVbo = 0;
    GLuint m_mslVao = 0, m_mslVbo = 0;
    GLsizei m_jetCount = 0, m_flameCount = 0, m_mslCount = 0;
};
