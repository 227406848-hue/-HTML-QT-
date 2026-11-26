# 第三方素材

## 背景音乐

**5 Chiptunes (Action)** — 作者 Juhani Junkala（OpenGameArt 用户名 SubspaceAudio）
- 页面：https://opengameart.org/content/5-chiptunes-action
- 授权：**CC0 1.0（公有领域）**，可商用、可修改、无需署名（这里还是写上，出于礼貌）
- 原始文件：`assets-src/*.mp3`
- 本项目用到的：`assets/music/theme.wav`（由 level1.mp3 转码：44.1kHz / 16bit / 立体声，峰值归一到 -3dB，去掉了首尾静音）
- 备选：`assets/music/title.wav`（title screen.mp3，14 秒短循环）

替换背景音乐：只要把同名 44.1kHz / 16bit PCM 的 WAV 放到 `assets/music/theme.wav` 即可，代码不用改。
