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

## 踩过的坑

1. `beforeRendering` 直接画默认帧缓冲 → 被 RHI 清屏覆盖。
2. `grabWindow()` 走离屏重绘 → 抓不到裸 GL 内容。
3. 原版 shader 输出 `vec4(col, 0.0)`，WebGL canvas 用 `alpha:false` 看不出问题，
   但在 QML 里就是全透明 → 改成 alpha 1.0。
4. GL 的 `uv.y=0` 在屏幕底部，天空渐变要翻。
7. 机影前后颠倒：shader 里机影形状把机头摆在 +lz，而地面机头投影落在 -lz，`uPFwd` 要传**反向**。
8. 座舱视角必须不画机体，否则眼点在机身里会糊满屏幕（原版用 `wCockpit` 阈值做同样的事）。
10. 水面高度不能直接用海底高度：深水区 `sampleHeight` 是负数（能到 -42），
    空气墙的 floor 必须按 `max(seabed, 0) + 9.5` 算，否则飞机真的会沉到水面以下。
11. 浪花改成线条（WaterWake）：复用 Trail 的 11-float 顶点布局和 trail shader，
    只是把 uWidth 调细（1.1）、uMaxW 压到 3.6、uPS 换成像素尺度——否则线宽被 9px 上限糊成实心块。
12. Qt 里 `emit` 是宏（信号关键字），自己的函数别叫 emit。
13. **黑屏根源（M9 踩到）**：`updatePaintNode` 里先 `uploadWorld` 再 `render`，
    而 `uploadWorld` 内部依赖 `m_inited`（GL 初始化在 render 里）。世界生成得比首帧快时
    （加载背景音乐让启动慢了 300ms 就会这样），首帧 upload 直接 return 但 `m_uploaded` 还是被置 true，
    地形永远传不上去 → 只剩 HUD、场景全黑。修法：`uploadWorld` 内部先 `ensureInit()` 并返回是否成功，
    调用方只在成功时才置 `m_uploaded`。
14. QML 里 `gameItem.muted` 这种写法拿到的是函数对象（永远为真），要用 property 或加括号调用。
15. 免费素材用 CC0：`opengameart.org/content/5-chiptunes-action`（Juhani Junkala），
    下载是 mp3，用 `soundfile`（libsndfile 自带 mp3 解码）转成 44.1k/16bit WAV 再给 waveOut 播。
16. WinMM waveOut 三块缓冲一开始没人写就永远不会置 WHDR_DONE：启动时必须先自己 mix 一遍再
    waveOutWrite，否则音频线程永远在等「空闲块」，一声不响。
17. 音频线程要 `timeBeginPeriod(1)` + `THREAD_PRIORITY_TIME_CRITICAL`：默认优先级下会被
    渲染线程抢 CPU，实测只跑到 31k/44.1k 采样，声音会断断续续。
18. 粒子点尺寸不能写死 `uPS=1`：
    `gl_PointSize = size * uPS / 距离`，uPS 必须等于 `半屏高 / tan(fov/2)`（本项目 300px / tan(0.49) ≈ 281），
    否则所有粒子退化成 1 像素的点——水花、火球全看不见。尺寸按世界单位给：水花 0.5~1.7，爆炸 28~70。
11. 水花粒子要抬高生成（sea+2.5）并沿翼展布点，否则会被波面吃掉、或者只剩一颗孤点。
9. 机影朝向不要靠眼睛判断：写个脚本复刻 `planeShadow()` 的数学，统计「机头段/机尾段」两个区域的落点质心在 forward 上的投影，才知道符号对不对（本次结论：必须传 `-forward`）。
5. Qt 6 已移除 `QQuickWindow::defaultFramebufferObject()`。

## 已知缺口

- 本机没装 Qt Multimedia，暂时静音。
