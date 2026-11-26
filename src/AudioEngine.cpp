#include "AudioEngine.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int kRate = 44100;
constexpr int kFrames = 512;      // 每块 11.6ms
constexpr int kBuffers = 4;       // 四块轮转，约 46ms 总缓冲
constexpr int kMaxVoices = 6;
constexpr float kPi = 3.14159265358979f;
constexpr float kMusicGain = 0.30f;   // 背景音乐压低一点，别盖住爆炸

inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// 一次性音效（爆炸）
struct Voice
{
    bool live = false;
    float pan = 0.0f;
    float gain = 1.0f;
    float t = 0.0f;
    float dur = 1.6f;
    float phase = 0.0f;
    float freq = 95.0f;
    float lp = 0.0f;
};

unsigned readU32(const unsigned char *p)
{
    return unsigned(p[0]) | (unsigned(p[1]) << 8) | (unsigned(p[2]) << 16) | (unsigned(p[3]) << 24);
}

unsigned readU16(const unsigned char *p)
{
    return unsigned(p[0]) | (unsigned(p[1]) << 8);
}

} // namespace

struct AudioEngine::Impl
{
    HWAVEOUT dev = nullptr;
    WAVEHDR hdr[kBuffers] = {};
    std::vector<short> pcm[kBuffers];
    HANDLE evt = nullptr;
    std::thread thread;

    std::mutex mtx;
    AudioParams params;
    std::atomic<long long> framesDone{0};

    std::vector<short> music;       // 交错立体声，44.1kHz
    size_t musicPos = 0;
    float duck = 1.0f;              // 爆炸时给音乐让路
    float masterS = 0.8f;

    Voice voices[kMaxVoices];
    unsigned noiseState = 0x9e3779b9u;

    float noise()
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return float(noiseState & 0xffffff) / 8388608.0f - 1.0f;
    }

    void mix(short *dst, int frames);
};

AudioEngine::AudioEngine()
    : m_impl(new Impl)
{
}

AudioEngine::~AudioEngine()
{
    stop();
}

long long AudioEngine::framesPlayed() const
{
    return m_impl->framesDone.load();
}

bool AudioEngine::hasMusic() const
{
    return !m_impl->music.empty();
}

// 读 16bit PCM WAV；顺便把单声道摊成双声道、把非 44.1k 线性重采样
bool AudioEngine::loadMusic(const std::string &path)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::vector<unsigned char> buf;
    {
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n < 44) {
            std::fclose(f);
            return false;
        }
        buf.resize(size_t(n));
        const size_t got = std::fread(buf.data(), 1, buf.size(), f);
        std::fclose(f);
        buf.resize(got);
    }

    if (buf.size() < 44 || std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0)
        return false;

    int channels = 0, rate = 0, bits = 0;
    const unsigned char *data = nullptr;
    size_t dataLen = 0;

    size_t off = 12;
    while (off + 8 <= buf.size()) {
        const unsigned char *id = buf.data() + off;
        const unsigned len = readU32(buf.data() + off + 4);
        const unsigned char *body = buf.data() + off + 8;
        if (off + 8 + len > buf.size())
            break;
        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16) {
            channels = int(readU16(body + 2));
            rate = int(readU32(body + 4));
            bits = int(readU16(body + 14));
        } else if (std::memcmp(id, "data", 4) == 0) {
            data = body;
            dataLen = len;
        }
        off += 8 + len + (len & 1);
    }

    if (!data || channels < 1 || bits != 16 || rate <= 0)
        return false;

    const size_t frames = dataLen / size_t(channels * 2);
    std::vector<short> pcmIn(frames * size_t(channels));
    std::memcpy(pcmIn.data(), data, frames * size_t(channels) * 2);

    // 目标：交错立体声 44.1k
    const double step = double(rate) / double(kRate);
    const size_t outFrames = size_t(double(frames) / step);
    std::vector<short> out(outFrames * 2);

    for (size_t i = 0; i < outFrames; ++i) {
        const double srcF = double(i) * step;
        const size_t i0 = std::min(size_t(srcF), frames - 1);
        const size_t i1 = std::min(i0 + 1, frames - 1);
        const float frac = float(srcF - double(i0));

        float l = 0.0f, r = 0.0f;
        const short a0 = pcmIn[i0 * size_t(channels)];
        const short a1 = pcmIn[i1 * size_t(channels)];
        l = float(a0) + (float(a1) - float(a0)) * frac;
        if (channels >= 2) {
            const short b0 = pcmIn[i0 * size_t(channels) + 1];
            const short b1 = pcmIn[i1 * size_t(channels) + 1];
            r = float(b0) + (float(b1) - float(b0)) * frac;
        } else {
            r = l;
        }
        out[i * 2 + 0] = short(clampf(l, -32768.0f, 32767.0f));
        out[i * 2 + 1] = short(clampf(r, -32768.0f, 32767.0f));
    }

    {
        std::lock_guard<std::mutex> lock(m_impl->mtx);
        m_impl->music.swap(out);
        m_impl->musicPos = 0;
    }
    return true;
}

void AudioEngine::setParams(const AudioParams &p)
{
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    m_impl->params = p;
}

void AudioEngine::explosion(const Vec3 &pos, float size)
{
    Impl *d = m_impl.get();
    std::lock_guard<std::mutex> lock(d->mtx);

    const Vec3 rel{pos.x - d->params.camPos.x, pos.y - d->params.camPos.y, pos.z - d->params.camPos.z};
    const float dist = std::sqrt(rel.x * rel.x + rel.y * rel.y + rel.z * rel.z);
    float pan = 0.0f;
    if (dist > 1.0f) {
        pan = (rel.x * d->params.camRight.x + rel.z * d->params.camRight.z) / dist;
        pan = clampf(pan, -1.0f, 1.0f);
    }
    const float atten = 1.0f / (1.0f + dist / 160.0f);

    int slot = -1;
    for (int i = 0; i < kMaxVoices; ++i) {
        if (!d->voices[i].live) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        float oldest = -1.0f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (d->voices[i].t > oldest) {
                oldest = d->voices[i].t;
                slot = i;
            }
        }
    }

    Voice &v = d->voices[slot];
    v.live = true;
    v.pan = pan;
    v.gain = clampf(size, 0.2f, 2.0f) * atten;
    v.t = 0.0f;
    v.dur = 1.6f;
    v.phase = 0.0f;
    v.freq = 95.0f;
    v.lp = 0.0f;
}

bool AudioEngine::start()
{
    if (m_running.load())
        return true;

    Impl *d = m_impl.get();

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = WORD(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    d->evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!d->evt)
        return false;

    const MMRESULT ok = waveOutOpen(&d->dev, WAVE_MAPPER, &fmt,
                                    reinterpret_cast<DWORD_PTR>(d->evt), 0, CALLBACK_EVENT);
    if (ok != MMSYSERR_NOERROR) {
        CloseHandle(d->evt);
        d->evt = nullptr;
        d->dev = nullptr;
        return false;
    }

    for (int i = 0; i < kBuffers; ++i) {
        d->pcm[i].assign(size_t(kFrames) * 2, 0);
        WAVEHDR &h = d->hdr[i];
        h = WAVEHDR{};
        h.lpData = reinterpret_cast<LPSTR>(d->pcm[i].data());
        h.dwBufferLength = DWORD(size_t(kFrames) * 2 * sizeof(short));
        waveOutPrepareHeader(d->dev, &h, sizeof(WAVEHDR));
        // 先自己填一遍再交出去，否则 WHDR_DONE 永不置位，音频线程等不到空闲块
        d->mix(reinterpret_cast<short *>(h.lpData), kFrames);
        waveOutWrite(d->dev, &h, sizeof(WAVEHDR));
    }

    // 诊断开关：DP_AUDIO_NOTHREAD 只开设备不启线程；DP_AUDIO_NOTIMER 不动系统定时器
    const bool noThread = std::getenv("DP_AUDIO_NOTHREAD") != nullptr;
    const bool noTimer = std::getenv("DP_AUDIO_NOTIMER") != nullptr;
    if (!noTimer)
        timeBeginPeriod(1);

    m_running.store(true);
    if (noThread)
        return true;

    d->thread = std::thread([this, d, noTimer] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        while (m_running.load()) {
            bool fed = false;
            for (int i = 0; i < kBuffers; ++i) {
                WAVEHDR &h = d->hdr[i];
                if (h.dwFlags & WHDR_DONE) {
                    h.dwFlags &= ~WHDR_DONE;
                    d->mix(reinterpret_cast<short *>(h.lpData), kFrames);
                    h.dwBufferLength = DWORD(size_t(kFrames) * 2 * sizeof(short));
                    waveOutWrite(d->dev, &h, sizeof(WAVEHDR));
                    fed = true;
                }
            }
            if (!fed)
                WaitForSingleObject(d->evt, 2);
        }
        if (!noTimer)
            timeEndPeriod(1);
    });
    return true;
}

void AudioEngine::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);
    if (m_impl->thread.joinable())
        m_impl->thread.join();

    Impl *d = m_impl.get();
    if (d->dev) {
        waveOutReset(d->dev);
        for (int i = 0; i < kBuffers; ++i)
            waveOutUnprepareHeader(d->dev, &d->hdr[i], sizeof(WAVEHDR));
        waveOutClose(d->dev);
        d->dev = nullptr;
    }
    if (d->evt) {
        CloseHandle(d->evt);
        d->evt = nullptr;
    }
}

// 一路混音：背景音乐（循环、爆炸时让路）+ 所有在响的爆炸
void AudioEngine::Impl::mix(short *dst, int frames)
{
    AudioParams p;
    size_t musicAt = 0;
    {
        std::lock_guard<std::mutex> lock(mtx);
        p = params;
        musicAt = musicPos;
    }

    const size_t musicFrames = music.size() / 2;

    for (int i = 0; i < frames; ++i) {
        float musicL = 0.0f, musicR = 0.0f;
        if (musicFrames > 0) {
            musicL = float(music[musicAt * 2 + 0]) / 32768.0f;
            musicR = float(music[musicAt * 2 + 1]) / 32768.0f;
            musicAt = (musicAt + 1) % musicFrames;
        }

        const float nz = noise();
        float fxL = 0.0f, fxR = 0.0f;
        bool anyLive = false;

        for (int v = 0; v < kMaxVoices; ++v) {
            Voice &vo = voices[v];
            if (!vo.live)
                continue;
            anyLive = true;

            vo.t += 1.0f / float(kRate);
            if (vo.t >= vo.dur) {
                vo.live = false;
                continue;
            }

            vo.freq = 95.0f - 60.0f * (vo.t / vo.dur);          // 冲击声往下扫
            vo.phase += vo.freq / float(kRate);
            if (vo.phase >= 1.0f)
                vo.phase -= 1.0f;
            vo.lp += (nz - vo.lp) * 0.10f;

            const float env = std::exp(-vo.t * 3.0f);
            const float boom = std::sin(2.0f * kPi * vo.phase) * std::exp(-vo.t * 2.4f) * 0.75f
                             + vo.lp * 1.5f * env;
            const float crack = (vo.t < 0.06f) ? nz * std::exp(-vo.t * 70.0f) * 0.9f : 0.0f;
            const float smp = (boom + crack) * vo.gain * 0.55f;

            const float ang = (vo.pan + 1.0f) * 0.25f * kPi;    // 等功率声像
            fxL += smp * std::cos(ang);
            fxR += smp * std::sin(ang);
        }

        // 爆炸时音乐让路，冲击才清楚；平滑过渡避免音量跳变
        const float duckTarget = anyLive ? 0.45f : 1.0f;
        duck += (duckTarget - duck) * 0.0025f;
        masterS += (p.master - masterS) * 0.002f;

        const float g = p.mute ? 0.0f : masterS;
        const float l = musicL * kMusicGain * duck + fxL;
        const float r = musicR * kMusicGain * duck + fxR;

        dst[i * 2 + 0] = short(clampf(std::tanh(l * g), -1.0f, 1.0f) * 32767.0f);
        dst[i * 2 + 1] = short(clampf(std::tanh(r * g), -1.0f, 1.0f) * 32767.0f);
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        musicPos = musicAt;
    }
    framesDone += frames;
}

// 离线渲染一段试听：背景音乐 + 一次爆炸，不开声卡也能验
bool AudioEngine::renderDemoWav(const char *path, float seconds)
{
    Impl *d = m_impl.get();
    const int total = int(seconds * float(kRate));
    std::vector<short> all(size_t(total) * 2, 0);

    for (int done = 0; done < total; done += kFrames) {
        const float t = float(done) / float(kRate);
        if (t >= 2.2f && t < 2.2f + float(kFrames) / float(kRate))
            explosion(Vec3{200.0f, 0.0f, 140.0f}, 1.3f);

        const int n = std::min(kFrames, total - done);
        d->mix(all.data() + size_t(done) * 2, n);
    }

    std::FILE *f = std::fopen(path, "wb");
    if (!f)
        return false;

    const unsigned dataBytes = unsigned(all.size() * sizeof(short));
    unsigned char hdr[44] = {};
    auto put32 = [&](int off, unsigned v) {
        hdr[off] = v & 0xff; hdr[off + 1] = (v >> 8) & 0xff;
        hdr[off + 2] = (v >> 16) & 0xff; hdr[off + 3] = (v >> 24) & 0xff;
    };
    auto put16 = [&](int off, unsigned v) {
        hdr[off] = v & 0xff; hdr[off + 1] = (v >> 8) & 0xff;
    };
    std::memcpy(hdr, "RIFF", 4);
    put32(4, 36 + dataBytes);
    std::memcpy(hdr + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, 2);
    put32(24, kRate);
    put32(28, kRate * 4);
    put16(32, 4);
    put16(34, 16);
    std::memcpy(hdr + 36, "data", 4);
    put32(40, dataBytes);

    std::fwrite(hdr, 1, sizeof(hdr), f);
    std::fwrite(all.data(), sizeof(short), all.size(), f);
    std::fclose(f);
    return true;
}
