# Dusk Patrol — Qt / OpenGL 重写

原作为 Claude 生成的 WebGL2 作品 **Dusk Patrol**
本项目用 C++ / OpenGL 3.3 core + Qt 6 重写渲染核心，属于**二次开发**，不是原创。

## 环境

- Qt 6.12.0 (mingw_64) + MinGW 13.1：`D:\Qt`
- Qt Creator 20.0.2
- 用 Qt Creator 打开目录的 `CMakeLists.txt`，Kit 选「Qt 6.12.0 MinGW 64-bit」

## 构建与自检

```powershell
.\scripts\build.ps1        # Ninja 构建
.\scripts\snapshot.ps1     # 渲染 240 帧后存 docs\snapshot.png
```

运行日志写在 exe 旁边的 `dusk-patrol.log`（GUI 程序在 Windows 上看不到 stdout）。

## 架构

画面不走「直接画屏幕」：Qt 6 的 RHI 每帧清屏重绘整个场景，在 `beforeRendering` 里画到默认帧缓冲会被整块盖掉。

```
GameItem::updatePaintNode        （渲染线程，GL 上下文 current）
  ├─ Renderer::render()          1/4 分辨率 FBO：地形 raymarch + 后续的飞机/尾迹
  ├─ Renderer::takeFrame()       glReadPixels -> QImage（注意 alpha 必须是 1）
  └─ createTextureFromImage      QSGSimpleTextureNode，Nearest 过滤
QML 叠 HUD
```

## 目录

```
src/World.*       噪声与地形/云层生成（2048² 高度场，16 线程，约 0.1s）
src/FlightModel.* 飞行状态机（yaw/pitch/roll + omega 阻尼，移植自原版 update）
src/Mesh.*        飞机程序化网格（照原版 buildPlane：进气道/翼型放样/外倾双垂尾）
src/Trail.*       翼尖尾迹丝带（环形采样 + 屏幕空间带状展开）
src/Particles.*   爆炸粒子（火球/烟/水花/闪光，GL_POINTS）
src/SceneShader.h 地形 raymarch 着色器（原版 GLSL ES 300 移植到 330 core）
src/Renderer.*    裸 GL：FBO、program、纹理上传、绘制、回读
src/GameItem.*    纹理桥 + 世界生成的异步调度
src/GameWindow.*  QQuickView 子类：fps、快照
docs/orig/        从网页版扒下来的原始 shader / world.js，供对照
```



## 操作

- 鼠标位置 = 摇杆（左右横滚 / 上下俯仰）
- 按住**左键** = 加力
- **左键双击** = 花式 360° 翻滚
- **右键** = 发射导弹（4 个挂架：翼尖 2 + 翼下 2，打完 1 秒补挂）
- **T** = 切换视角（俯视 → 追尾 → 座舱）
- **空格** = 特技：单击 180° 倒飞并保持，再单击翻回来；双击 360° 翻滚
- 环境变量：`DP_AUTOFIRE=1` 自动开火+翻滚；`DP_VIEW=0/1/2` 指定视角；`DP_HOLD_BOOST=1` 常按加力；`DP_TACTIC=1` 自动按一次空格（验证倒飞）；`DP_LOWALT=1` 压低高度（看地面投影）；`DP_SKIM=1` 压杆贴水面飞（验证浪花与空气墙）


