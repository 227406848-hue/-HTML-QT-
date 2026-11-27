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


复刻提示词：
Build "Dusk Patrol": a pixel-art fighter-jet flying toy that runs in the browser as one self-contained HTML file. No libraries and no image assets: raw WebGL2 for the world, Canvas 2D for the cockpit, everything drawn by code. Google Fonts are the only external resource (a pixel display face for the title, a monospace face for hints). This should feel like a polished indie game demo, not a tech test, so spend your effort on how it looks and how it feels to fly.

## LOOK
- Render at a low internal resolution (about 340 px tall) and upscale with nearest-neighbour so pixels stay crisp. Limited palette, ordered 4x4 Bayer dithering between tones, no anti-aliasing, no smooth gradients.
- Setting: lake country in autumn at the last light of day. Warm low sun, violet shadows, peach clouds, haze toward the horizon that is gold toward the sun and lilac away from it.

## WORLD
- Generate a seamlessly tiling height field at load (use web workers and show a progress bar), then ray-march it per pixel in a fragment shader so any camera angle works.
- Water: depth bands from pale turquoise shallows to navy, animated foam along shores, short white glints, sky reflection at low angles and a glitter path under the sun.
- Land: sand, pale meadows, forest in patches of orange, crimson and gold with scattered dark conifers, grey-violet rock, snow on the highest peaks, subtle terraced shading, long baked shadows.
- A layer of puffy cumulus below cruise altitude, shaded in stepped tones, casting shadows on the ground. Sky gradient with a large sun disc and thin high streaks.

## AIRCRAFT
- An original twin-engine, twin-tail fighter: pointed dark radome, gold-tinted bubble canopy, cropped delta wings, canted fins with red caps, glowing nozzles, grey and slate splinter camouflage.
- Low-poly mesh, flat-shaded in three tones, with a one-pixel dark outline.
- Thin white vapour trails from the wingtips, twin afterburner plumes with a white core, the jet's shadow on ground and clouds, spray and a wake when skimming water.

## CAMERAS AND CONTROLS
- Three views cycled with Space, blending smoothly over about a second: top-down (north up), chase, and cockpit.
- Mouse steers. In the top view the jet flies toward the cursor; in chase and cockpit the cursor's offset from centre sets turn and climb or dive. Click: barrel roll. Hold: afterburner. Right click or F: fire a missile. Left and right buttons must work together.
- Arcade flight: it cannot crash, it pulls up on its own before terrain, and it flies lazy S-turns until the player first moves the mouse. Add touch controls with on-screen buttons.

## COCKPIT
- A hand-plotted pixel instrument panel where every reading is real: airspeed, artificial horizon, altimeter with digits, turn and slip, heading, vertical speed, an amber ground-mapping radar with a sweep, engine gauges, a clock showing local time, warning lamps, a throttle lever that follows the afterburner.
- A head-up display whose pitch ladder sits exactly on the real horizon outside, with heading tape, speed, altitude, G, radar height, missile count and warnings.

## MISSILES
- Four on the wings; each leaves its rail when fired and re-arms after a few seconds. They fly with a flame and a smoke trail to where the nose points, or to the ground ahead when flying level.
- Impact: flash, fireball, lingering smoke and an expanding shock ring; a scorch with embers on land, a column of spray and foam on water.

## INTERFACE
- All text in English: a title card, one quiet strip of control hints with a missile counter, and a sound toggle.
- Sound is synthesised with Web Audio and off by default.

## QUALITY BAR
- Smooth on an integrated GPU; drop internal resolution automatically if frames get slow.
- Before you call it done, run it in a headless browser, screenshot all three views plus a missile strike, and fix whatever looks wrong. Simulate several minutes of random flying and input to confirm there are no errors.
