#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "GameState.h"

// 每帧从游戏线程推给音频线程的状态
struct AudioParams
{
    Vec3 camPos;
    Vec3 camRight{1, 0, 0};
    bool mute = false;
    float master = 0.8f;
};

// 软件混音器：WinMM waveOut 输出。背景音乐读一个 44.1k/16bit 的 WAV 循环播，
// 导弹爆炸是实时合成的（噪声 + 扫频低频），两边在同一路混音里相加。
class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool running() const { return m_running.load(); }
    long long framesPlayed() const;

    // 载入背景音乐（16bit PCM WAV；单声道会自动摊成双声道，采样率不是 44.1k 会线性重采样）
    bool loadMusic(const std::string &path);
    bool hasMusic() const;

    void setParams(const AudioParams &p);

    // 在某个世界坐标炸一下（按距离衰减 + 左右声像）
    void explosion(const Vec3 &pos, float size = 1.0f);

    // 离线渲染一段试听（背景音乐 + 一次爆炸），不需要声卡
    bool renderDemoWav(const char *path, float seconds = 5.0f);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<bool> m_running{false};
};
