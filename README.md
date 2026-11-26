# Dusk Patrol — Qt / OpenGL 重写

原作为 Claude 生成的 WebGL2 作品 **Dusk Patrol**（网页版源码在 `D:\workspace\games\dusk-patrol`）。
本项目用 C++ / OpenGL 3.3 core + Qt 6 重写渲染核心，属于**二次开发**，不是原创。

## 环境

- Qt 6.12.0 (mingw_64) + MinGW 13.1：`D:\Qt`
- Qt Creator 20.0.2：`D:\Qt\Tools\QtCreator\bin\qtcreator.exe`
- 用 Qt Creator 打开本目录的 `CMakeLists.txt`，Kit 选「Qt 6.12.0 MinGW 64-bit」

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

## 进度

- [x] M0 环境打通
- [x] M1 像素化天空 + FPS 叠加
- [x] M2 地形（程序化高度场 + raymarch + 云 + 烘焙阴影）
- [x] M3 飞行模型 + 追尾相机 + 鼠标输入 + 飞机网格（docs/m3-flight.png）
- [x] M4-a 战斗机机体（照原版 buildPlane 移植）+ 导弹 + 加力尾焰 + 爆炸标记
- [x] M4-b 翼尖尾迹丝带 + 爆炸粒子（火球/烟/水花/闪光）+ 落地冲击环
- [x] M5 三视角（T 循环，0.9s 平滑过渡）+ 座舱 HUD（速度/高度/燃油）
- [x] M5-b 挂载导弹：4 个挂架，打完 1 秒补挂
- [x] M5-c 燃油：满油门 10 秒耗尽，松开约 3.5 秒回满（只在座舱 HUD 显示）
- [x] M6 座舱仪表盘（航向条 / 准星 / 雷达 / 速度·高度·罗盘 / 燃油条）
- [x] M6-b 空格特技：单击 180° 倒飞，双击 360° 翻滚
- [x] M6-c 俯视视角随航向旋转 + 转率提升（1.15 → 1.85 rad/s）
- [x] M7 HUD 详细化：速度带 / 高度带 / 俯仰梯 / 飞行路径标记 / 坡度尺 / 航向带 / 数据块 / 告警
- [x] M7-b 仪表盘：雷达、ADI 姿态仪、HSI 罗盘、RPM/EGT、燃油条、六格警告灯
- [x] M8 空气墙：水面最低 9.5 / 陆地 13（水面按 y=0 算，不跟海底沉下去），撞墙自动把姿态往上推
- [x] M8-b 掠水浪花：试过粒子版和线条版，最后按需求整块移除（WaterWake 已删，只剩空气墙）
- [x] M9 音效：WinMM waveOut 软件混音器
  - 背景音乐：CC0 芯片音乐（Juhani Junkala《5 Chiptunes (Action)》）循环播放，见 `assets/CREDITS.md`
  - 导弹爆炸：实时合成（扫频低频冲击 + 低通噪声 + 起手爆裂），按距离衰减、按左右声像分配
  - 爆炸时背景音乐自动让路（ducking），软削波（tanh）防失真
  - 44.1kHz / 16bit / 立体声，4 块 × 512 帧轮转缓冲（约 46ms）；音频线程 TIME_CRITICAL
  - `--audio-render out.wav` 离线渲染试听（不开窗口、不碰声卡）；M 键静音
- [ ] M9 音效（需补 Qt Multimedia）
- [ ] M5 座舱 HUD
- [ ] M6 打磨

## 操作

- 鼠标位置 = 摇杆（左右横滚 / 上下俯仰）
- 按住**左键** = 加力
- **左键双击** = 花式 360° 翻滚
- **右键** = 发射导弹（4 个挂架：翼尖 2 + 翼下 2，打完 1 秒补挂）
- **T** = 切换视角（俯视 → 追尾 → 座舱）
- **空格** = 特技：单击 180° 倒飞并保持，再单击翻回来；双击 360° 翻滚
- 环境变量：`DP_AUTOFIRE=1` 自动开火+翻滚；`DP_VIEW=0/1/2` 指定视角；`DP_HOLD_BOOST=1` 常按加力；`DP_TACTIC=1` 自动按一次空格（验证倒飞）；`DP_LOWALT=1` 压低高度（看地面投影）；`DP_SKIM=1` 压杆贴水面飞（验证浪花与空气墙）


