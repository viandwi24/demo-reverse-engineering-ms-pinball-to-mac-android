// Mixer WAV 8 kanal di atas SDL, meniru WaveMix (11025 Hz, 8-bit, mono, dijumlah lalu di-clip).
#include <SDL.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "audio.h"

namespace audio {
namespace {

constexpr int kChannels = 8;

struct Channel {
    const std::vector<uint8_t>* data = nullptr;
    size_t pos = 0;
    uint32_t start_time = 0;
};

SDL_AudioDeviceID g_device = 0;
std::string g_data_dir;
std::map<std::string, std::vector<uint8_t>> g_waves;
Channel g_channels[kChannels];
std::mutex g_mutex;
bool g_sound_on = true;

void mix(void*, Uint8* stream, int len) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int i = 0; i < len; ++i) {
        int sum = 0;
        for (Channel& c : g_channels) {
            if (!c.data || c.pos >= c.data->size()) continue;
            sum += static_cast<int>((*c.data)[c.pos++]) - 128;
        }
        stream[i] = static_cast<Uint8>(std::clamp(sum, -128, 127) + 128);
    }
}

const std::vector<uint8_t>* load_wave(const std::string& name) {
    auto it = g_waves.find(name);
    if (it != g_waves.end()) return &it->second;
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 len = 0;
    std::vector<uint8_t> pcm;
    if (SDL_LoadWAV((g_data_dir + "/" + name).c_str(), &spec, &buf, &len)) {
        // File asli: PCM 8-bit mono 11025 Hz; konversi bila berbeda.
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_U8, 1, 11025) > 0) {
            cvt.len = static_cast<int>(len);
            std::vector<uint8_t> tmp(static_cast<size_t>(len) * cvt.len_mult);
            std::memcpy(tmp.data(), buf, len);
            cvt.buf = tmp.data();
            SDL_ConvertAudio(&cvt);
            pcm.assign(tmp.begin(), tmp.begin() + cvt.len_cvt);
        } else {
            pcm.assign(buf, buf + len);
        }
        SDL_FreeWAV(buf);
    }
    return &(g_waves[name] = std::move(pcm));
}

}  // namespace

bool init(const std::string& data_dir) {
    g_data_dir = data_dir;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec want{}, have{};
    want.freq = 11025;
    want.format = AUDIO_U8;
    want.channels = 1;
    want.samples = 512;
    want.callback = mix;
    g_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!g_device) return false;
    SDL_PauseAudioDevice(g_device, 0);
    return true;
}

void shutdown() {
    music_stop();
    if (g_device) SDL_CloseAudioDevice(g_device);
    g_device = 0;
}

void play_wave(const std::string& wav_name) {
    if (!g_device || !g_sound_on || wav_name.empty()) return;
    const std::vector<uint8_t>* data = load_wave(wav_name);
    std::lock_guard<std::mutex> lock(g_mutex);
    int best = 0;
    for (int i = 1; i < kChannels; ++i)
        if (g_channels[i].start_time < g_channels[best].start_time) best = i;
    g_channels[best] = Channel{data, 0, SDL_GetTicks()};
}

void set_sound_enabled(bool on) { g_sound_on = on; }

}  // namespace audio
