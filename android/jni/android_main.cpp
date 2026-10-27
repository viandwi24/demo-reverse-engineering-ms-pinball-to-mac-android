// Android front-end (NativeActivity + native_app_glue) for the Space Cadet port.
// Rendering: ANativeWindow software buffer (RGBX_8888) with letterboxing; the compositor scales it.
// Audio: AAudio, 8-channel WaveMix-style mixer at 11025 Hz. Music: android.media.MediaPlayer via JNI.
#include <aaudio/AAudio.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "dat_file.h"
#include "exe_resources.h"
#include "engine/loader.h"
#include "engine/render.h"
#include "engine/table.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "pinball", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "pinball", __VA_ARGS__)

namespace {

constexpr const char* kAssetVersion = "3";  // 3: Pinball.exe ikut disalin (resource string + font)

int64_t now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

// ---------------------------------------------------------------- assets -> internal storage
bool copy_assets(AAssetManager* mgr, const std::string& dir) {
    std::string marker = dir + "/.assets_version";
    if (FILE* f = std::fopen(marker.c_str(), "r")) {
        char buf[16] = {};
        std::fgets(buf, sizeof(buf), f);
        std::fclose(f);
        if (std::strcmp(buf, kAssetVersion) == 0) return true;
    }
    mkdir(dir.c_str(), 0700);
    AAssetDir* ad = AAssetManager_openDir(mgr, "");
    if (!ad) return false;
    int count = 0;
    while (const char* name = AAssetDir_getNextFileName(ad)) {
        AAsset* a = AAssetManager_open(mgr, name, AASSET_MODE_STREAMING);
        if (!a) continue;
        // Store under the lower-case name (the engine asks for e.g. "sound1.wav"; Android storage is
        // case-sensitive) and add a symlink with the original spelling ("PINBALL.DAT").
        std::string lname = name;
        for (char& c : lname) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string out = dir + "/" + lname;
        if (lname != name) {
            std::string link = dir + "/" + name;
            unlink(link.c_str());
            symlink(lname.c_str(), link.c_str());
        }
        FILE* f = std::fopen(out.c_str(), "wb");
        if (f) {
            char buf[65536];
            int n;
            while ((n = AAsset_read(a, buf, sizeof(buf))) > 0) std::fwrite(buf, 1, n, f);
            std::fclose(f);
            ++count;
        }
        AAsset_close(a);
    }
    AAssetDir_close(ad);
    LOGI("copied %d asset files to %s", count, dir.c_str());
    if (FILE* f = std::fopen(marker.c_str(), "w")) {
        std::fputs(kAssetVersion, f);
        std::fclose(f);
    }
    return count > 0;
}

// ---------------------------------------------------------------- settings / high scores
std::string g_settings_path;

void load_settings() {
    if (FILE* f = std::fopen(g_settings_path.c_str(), "r")) {
        char key[64], val[64];
        while (std::fscanf(f, "%63s %63[^\n]", key, val) == 2) {
            std::string k = key;
            if (k == "Players") pb::g_players = std::max(1, std::min(4, std::atoi(val)));
            else if (k.rfind("HighScore", 0) == 0 && k.size() == 10) {
                int i = k[9] - '0';
                if (i >= 0 && i < 5) {
                    int score = 0;
                    char name[32] = {};
                    if (std::sscanf(val, "%d %31[^\n]", &score, name) >= 1) {
                        highscore::g_table[i].score = score;
                        std::snprintf(highscore::g_table[i].name, sizeof(highscore::g_table[i].name), "%s", name);
                    }
                }
            }
        }
        std::fclose(f);
    }
}

void save_settings() {
    if (g_settings_path.empty()) return;
    if (FILE* f = std::fopen(g_settings_path.c_str(), "w")) {
        std::fprintf(f, "Players %d\n", pb::g_players);
        for (int i = 0; i < 5; ++i)
            std::fprintf(f, "HighScore%d %d %s\n", i, highscore::g_table[i].score, highscore::g_table[i].name);
        std::fclose(f);
    }
}

// ---------------------------------------------------------------- audio (AAudio mixer)
namespace audio {

constexpr int kChannels = 8;
struct Channel {
    const std::vector<uint8_t>* data = nullptr;
    size_t pos = 0;
    uint64_t serial = 0;
};

std::string g_data_dir;
std::map<std::string, std::vector<uint8_t>> g_waves;  // only touched from the game thread
Channel g_channels[kChannels];
std::mutex g_mutex;
uint64_t g_serial = 0;
AAudioStream* g_stream = nullptr;

uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }

// Parse a RIFF WAV into 8-bit unsigned mono 11025 Hz PCM.
std::vector<uint8_t> parse_wav(const std::vector<uint8_t>& f) {
    std::vector<uint8_t> out;
    if (f.size() < 12 || std::memcmp(f.data(), "RIFF", 4) || std::memcmp(f.data() + 8, "WAVE", 4)) return out;
    int fmt_tag = 1, chans = 1, rate = 11025, bits = 8;
    size_t p = 12;
    while (p + 8 <= f.size()) {
        uint32_t len = rd32(&f[p + 4]);
        const uint8_t* d = &f[p + 8];
        size_t avail = std::min<size_t>(len, f.size() - (p + 8));
        if (!std::memcmp(&f[p], "fmt ", 4) && avail >= 16) {
            fmt_tag = rd16(d);
            chans = std::max<int>(1, rd16(d + 2));
            rate = static_cast<int>(rd32(d + 4));
            bits = rd16(d + 14);
        } else if (!std::memcmp(&f[p], "data", 4)) {
            if (fmt_tag != 1) return out;
            int bps = bits / 8;
            size_t frames = avail / (bps * chans);
            std::vector<uint8_t> mono(frames);
            for (size_t i = 0; i < frames; ++i) {
                const uint8_t* s = d + i * bps * chans;  // first channel only
                mono[i] = bps == 1 ? s[0] : static_cast<uint8_t>((static_cast<int16_t>(rd16(s)) >> 8) + 128);
            }
            if (rate == 11025 || rate <= 0) return mono;
            size_t n = static_cast<size_t>(frames * 11025.0 / rate);
            out.resize(n);
            for (size_t i = 0; i < n; ++i) out[i] = mono[std::min(frames - 1, static_cast<size_t>(i * (double)rate / 11025))];
            return out;
        }
        p += 8 + len + (len & 1);
    }
    return out;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The data files are upper case but the engine asks for e.g. "sound1.wav" (Windows/macOS are case-insensitive).
std::string resolve_case(const std::string& name) {
    static std::map<std::string, std::string> files;
    if (files.empty()) {
        if (DIR* d = opendir(g_data_dir.c_str())) {
            while (dirent* e = readdir(d)) files[lower(e->d_name)] = e->d_name;
            closedir(d);
        }
    }
    auto it = files.find(lower(name));
    return g_data_dir + "/" + (it != files.end() ? it->second : name);
}

const std::vector<uint8_t>* load_wave(const std::string& name) {
    auto it = g_waves.find(name);
    if (it != g_waves.end()) return &it->second;
    std::vector<uint8_t> raw;
    if (FILE* f = std::fopen(resolve_case(name).c_str(), "rb")) {
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n > 0) {
            raw.resize(n);
            if (std::fread(raw.data(), 1, n, f) != static_cast<size_t>(n)) raw.clear();
        }
        std::fclose(f);
    } else {
        LOGE("missing wav %s", name.c_str());
    }
    return &(g_waves[name] = parse_wav(raw));
}

aaudio_data_callback_result_t callback(AAudioStream*, void*, void* audio_data, int32_t frames) {
    int16_t* out = static_cast<int16_t*>(audio_data);
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int32_t i = 0; i < frames; ++i) {
        int sum = 0;
        for (Channel& c : g_channels) {
            if (!c.data || c.pos >= c.data->size()) continue;
            sum += static_cast<int>((*c.data)[c.pos++]) - 128;
        }
        out[i] = static_cast<int16_t>(std::clamp(sum, -128, 127) * 256);
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void error_callback(AAudioStream*, void*, aaudio_result_t err) { LOGE("AAudio error %d", err); }

void open_stream() {
    if (g_stream) return;
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setSampleRate(b, 11025);
    AAudioStreamBuilder_setChannelCount(b, 1);
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDataCallback(b, callback, nullptr);
    AAudioStreamBuilder_setErrorCallback(b, error_callback, nullptr);
    aaudio_result_t r = AAudioStreamBuilder_openStream(b, &g_stream);
    AAudioStreamBuilder_delete(b);
    if (r != AAUDIO_OK) {
        LOGE("AAudio open failed: %s", AAudio_convertResultToText(r));
        g_stream = nullptr;
        return;
    }
    LOGI("AAudio stream: rate=%d fmt=%d", AAudioStream_getSampleRate(g_stream), AAudioStream_getFormat(g_stream));
    AAudioStream_requestStart(g_stream);
}

void close_stream() {
    if (!g_stream) return;
    AAudioStream_requestStop(g_stream);
    AAudioStream_close(g_stream);
    g_stream = nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (Channel& c : g_channels) c = Channel{};
}

void play_wave(const std::string& name) {
    if (!g_stream || name.empty()) return;
    const std::vector<uint8_t>* data = load_wave(name);
    std::lock_guard<std::mutex> lock(g_mutex);
    int best = 0;
    for (int i = 1; i < kChannels; ++i)
        if (g_channels[i].serial < g_channels[best].serial) best = i;
    g_channels[best] = Channel{data, 0, ++g_serial};
}

}  // namespace audio

// ---------------------------------------------------------------- music (MediaPlayer through JNI)
namespace music {

JNIEnv* g_env = nullptr;
jobject g_player = nullptr;
jmethodID g_start, g_pause, g_seek;
bool g_want_playing = false;

bool check(JNIEnv* env, const char* what) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("JNI exception in %s", what);
        return false;
    }
    return true;
}

void init(JNIEnv* env, const std::string& midi_path) {
    g_env = env;
    jclass cls = env->FindClass("android/media/MediaPlayer");
    if (!check(env, "FindClass") || !cls) return;
    jobject p = env->NewObject(cls, env->GetMethodID(cls, "<init>", "()V"));
    if (!check(env, "new MediaPlayer") || !p) return;
    jstring path = env->NewStringUTF(midi_path.c_str());
    env->CallVoidMethod(p, env->GetMethodID(cls, "setDataSource", "(Ljava/lang/String;)V"), path);
    env->DeleteLocalRef(path);
    if (!check(env, "setDataSource")) return;
    env->CallVoidMethod(p, env->GetMethodID(cls, "setLooping", "(Z)V"), JNI_TRUE);
    env->CallVoidMethod(p, env->GetMethodID(cls, "prepare", "()V"));
    if (!check(env, "prepare")) return;
    g_start = env->GetMethodID(cls, "start", "()V");
    g_pause = env->GetMethodID(cls, "pause", "()V");
    g_seek = env->GetMethodID(cls, "seekTo", "(I)V");
    g_player = env->NewGlobalRef(p);
    env->DeleteLocalRef(p);
    env->DeleteLocalRef(cls);
    LOGI("MediaPlayer ready for %s", midi_path.c_str());
}

void hook(bool play) {
    g_want_playing = play;
    if (!g_player) return;
    if (play) {
        g_env->CallVoidMethod(g_player, g_seek, 0);
        g_env->CallVoidMethod(g_player, g_start);
    } else {
        g_env->CallVoidMethod(g_player, g_pause);
    }
    check(g_env, "music hook");
}

void suspend() {
    if (g_player && g_want_playing) {
        g_env->CallVoidMethod(g_player, g_pause);
        check(g_env, "suspend");
    }
}

void resume() {
    if (g_player && g_want_playing) {
        g_env->CallVoidMethod(g_player, g_start);
        check(g_env, "resume");
    }
}

}  // namespace music

// ---------------------------------------------------------------- app state
struct App {
    android_app* app = nullptr;
    bool ready = false;  // table created
    bool focused = false;
    std::unique_ptr<DatFile> dat;
    std::array<uint32_t, 256> pal{};  // ABGR for RGBX_8888 memory layout (R in low byte)
    int buf_w = 0, buf_h = 0;          // window buffer geometry
    int off_x = 0, off_y = 0;          // where the 600x416 game image sits inside the buffer
    int64_t last_ms = 0;
    // touch: per pointer id -> control
    enum Ctl { None, Left, Right, Plunger, NewGame };
    std::map<int32_t, Ctl> pointers;
    int held[5] = {};
};

App g;

constexpr int kGameW = 600, kGameH = 416;

void setup_window_geometry() {
    ANativeWindow* win = g.app->window;
    if (!win) return;
    int ww = ANativeWindow_getWidth(win), wh = ANativeWindow_getHeight(win);
    if (ww <= 0 || wh <= 0) return;
    // Buffer with the window's aspect ratio that contains the 600x416 image at 1:1; the compositor scales it.
    if (static_cast<int64_t>(ww) * kGameH >= static_cast<int64_t>(wh) * kGameW) {
        g.buf_h = kGameH;
        g.buf_w = static_cast<int>((static_cast<int64_t>(ww) * kGameH + wh - 1) / wh);
    } else {
        g.buf_w = kGameW;
        g.buf_h = static_cast<int>((static_cast<int64_t>(wh) * kGameW + ww - 1) / ww);
    }
    g.buf_w = (g.buf_w + 1) & ~1;
    g.off_x = (g.buf_w - kGameW) / 2;
    g.off_y = (g.buf_h - kGameH) / 2;
    ANativeWindow_setBuffersGeometry(win, g.buf_w, g.buf_h, WINDOW_FORMAT_RGBX_8888);
    LOGI("window %dx%d -> buffer %dx%d", ww, wh, g.buf_w, g.buf_h);
}

void draw() {
    ANativeWindow* win = g.app->window;
    if (!win || !g.ready || g.buf_w == 0) return;
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(win, &buf, nullptr) != 0) return;
    const Bitmap8& frame = render::frame();
    int ox, oy;
    pb::render_offset(ox, oy);
    uint32_t* px = static_cast<uint32_t*>(buf.bits);
    for (int y = 0; y < buf.height; ++y) {
        uint32_t* row = px + static_cast<size_t>(y) * buf.stride;
        int gy = y - g.off_y - oy;
        for (int x = 0; x < buf.width; ++x) {
            int gx = x - g.off_x - ox;
            int fy = y - g.off_y, fx = x - g.off_x;
            if (fx >= 0 && fy >= 0 && fx < frame.width && fy < frame.height && gx >= 0 && gy >= 0 && gx < frame.width &&
                gy < frame.height)
                row[x] = g.pal[frame.pixels[gy * frame.stride + gx]];
            else
                row[x] = 0xff000000u;
        }
    }
    ANativeWindow_unlockAndPost(win);
}

void press(App::Ctl c, bool down) {
    int& h = g.held[c];
    if (down) {
        if (h++ > 0) return;
    } else {
        if (h == 0 || --h > 0) return;
    }
    switch (c) {
    case App::Left: down ? pb::keydown(90) : pb::keyup(90); break;
    case App::Right: down ? pb::keydown(191) : pb::keyup(191); break;
    case App::Plunger: down ? pb::keydown(32) : pb::keyup(32); break;
    case App::NewGame:
        if (down) pb::replay_level(0);
        break;
    default: break;
    }
}

App::Ctl classify(float x, float y) {
    ANativeWindow* win = g.app->window;
    if (!win) return App::None;
    float w = static_cast<float>(ANativeWindow_getWidth(win)), h = static_cast<float>(ANativeWindow_getHeight(win));
    float fx = x / w, fy = y / h;
    if (fx < 1.0f / 3) return App::Left;
    if (fx > 2.0f / 3) return App::Right;
    if (fy < 0.15f) return App::NewGame;
    if (fy > 0.5f) return App::Plunger;
    return App::None;
}

int to_vk(int32_t key) {
    if (key >= AKEYCODE_A && key <= AKEYCODE_Z) return 'A' + (key - AKEYCODE_A);
    if (key >= AKEYCODE_0 && key <= AKEYCODE_9) return '0' + (key - AKEYCODE_0);
    switch (key) {
    case AKEYCODE_SPACE: return 32;
    case AKEYCODE_SLASH: return 191;
    case AKEYCODE_PERIOD: return 190;
    case AKEYCODE_COMMA: return 188;
    case AKEYCODE_DPAD_UP: return 38;
    case AKEYCODE_DPAD_DOWN: return 40;
    case AKEYCODE_DPAD_LEFT: return 37;
    case AKEYCODE_DPAD_RIGHT: return 39;
    case AKEYCODE_ENTER: return 13;
    case AKEYCODE_SHIFT_LEFT: case AKEYCODE_SHIFT_RIGHT: return 16;
    case AKEYCODE_CTRL_LEFT: case AKEYCODE_CTRL_RIGHT: return 17;
    // gamepad
    case AKEYCODE_BUTTON_L1: return 90;
    case AKEYCODE_BUTTON_R1: return 191;
    case AKEYCODE_BUTTON_A: return 32;
    case AKEYCODE_BUTTON_X: return 88;
    case AKEYCODE_BUTTON_B: return 190;
    case AKEYCODE_BUTTON_Y: return 38;
    default: return 0;
    }
}

void release_all_touches() {
    for (auto& p : g.pointers) press(p.second, false);
    g.pointers.clear();
}

int32_t on_input(android_app*, AInputEvent* ev) {
    if (!g.ready) return 0;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(ev);
        int32_t a = action & AMOTION_EVENT_ACTION_MASK;
        size_t idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        switch (a) {
        case AMOTION_EVENT_ACTION_DOWN:
            release_all_touches();  // first finger: nothing can still be held
            [[fallthrough]];
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int32_t id = AMotionEvent_getPointerId(ev, idx);
            App::Ctl c = classify(AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx));
            g.pointers[id] = c;
            press(c, true);
            break;
        }
        case AMOTION_EVENT_ACTION_UP:  // last finger lifted: release everything
        case AMOTION_EVENT_ACTION_CANCEL:
            release_all_touches();
            break;
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            int32_t id = AMotionEvent_getPointerId(ev, idx);
            auto it = g.pointers.find(id);
            if (it != g.pointers.end()) {
                press(it->second, false);
                g.pointers.erase(it);
            }
            break;
        }
        default: break;
        }
        return 1;
    }
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        int32_t key = AKeyEvent_getKeyCode(ev);
        int32_t act = AKeyEvent_getAction(ev);
        if (key == AKEYCODE_BACK) return 0;  // default: finish activity
        bool down = act == AKEY_EVENT_ACTION_DOWN;
        if (down && AKeyEvent_getRepeatCount(ev) > 0) return 1;
        if (key == AKEYCODE_F2 || key == AKEYCODE_BUTTON_START) {
            if (down) pb::replay_level(0);
            return 1;
        }
        if (key == AKEYCODE_F3 || key == AKEYCODE_BUTTON_SELECT || key == AKEYCODE_P) {
            if (down) pb::pause_continue();
            return 1;
        }
        int vk = to_vk(key);
        if (!vk) return 0;
        down ? pb::keydown(vk) : pb::keyup(vk);
        return 1;
    }
    return 0;
}

void on_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        setup_window_geometry();
        g.last_ms = now_ms();
        draw();
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        setup_window_geometry();
        break;
    case APP_CMD_TERM_WINDOW:
        g.buf_w = 0;
        break;
    case APP_CMD_GAINED_FOCUS:
        g.focused = true;
        g.last_ms = now_ms();
        break;
    case APP_CMD_LOST_FOCUS:
        g.focused = false;
        release_all_touches();
        break;
    case APP_CMD_RESUME:
        if (g.ready) {
            audio::open_stream();
            music::resume();
        }
        break;
    case APP_CMD_PAUSE:
        audio::close_stream();
        music::suspend();
        save_settings();
        break;
    case APP_CMD_SAVE_STATE:
    case APP_CMD_DESTROY:
        save_settings();
        break;
    default: break;
    }
    (void)app;
}

bool init_game(android_app* app, JNIEnv* env) {
    std::string base = app->activity->internalDataPath ? app->activity->internalDataPath : "/data/local/tmp";
    mkdir(base.c_str(), 0700);
    std::string data_dir = base + "/data";
    if (!copy_assets(app->activity->assetManager, data_dir)) {
        LOGE("failed to copy assets");
        return false;
    }
    g.dat = DatFile::load(data_dir + "/PINBALL.DAT");
    if (!g.dat) {
        LOGE("failed to load %s/PINBALL.DAT", data_dir.c_str());
        return false;
    }
    if (!exe_resources::load(data_dir)) {
        LOGE("failed to read resources from %s/Pinball.exe", data_dir.c_str());
        return false;
    }
    pb::create_table(g.dat.get(), data_dir);
    const DatEntry* pal = g.dat->find("background", EntryType::Palette);
    for (int i = 0; i < 256 && pal; ++i) {
        const uint8_t* c = &pal->raw[i * 4];  // B G R x
        g.pal[i] = 0xff000000u | (c[0] << 16) | (c[1] << 8) | c[2];
    }
    LOGI("table created: %zu components", pb::g_main_table->component_list.size());

    audio::g_data_dir = data_dir;
    loader::set_sound_player([](const std::string& wav) { audio::play_wave(wav); });
    music::init(env, data_dir + "/PINBALL.MID");
    pb::g_music_hook = music::hook;

    g_settings_path = base + "/settings.txt";
    load_settings();
    audio::open_stream();
    pb::replay_level(0);
    g.ready = true;
    return true;
}

}  // namespace

void android_main(android_app* app) {
    g.app = app;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;

    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    if (!init_game(app, env)) LOGE("init failed");
    g.last_ms = now_ms();

    while (!app->destroyRequested) {
        bool active = g.ready && app->window && g.focused;
        int events;
        android_poll_source* source = nullptr;
        int timeout = active ? 0 : 100;
        while (ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
            timeout = 0;
        }
        if (app->destroyRequested) break;
        active = g.ready && app->window && g.focused;
        if (!active) {
            g.last_ms = now_ms();
            continue;
        }
        int64_t t0 = now_ms();
        int dt = static_cast<int>(std::min<int64_t>(t0 - g.last_ms, 100));
        g.last_ms = t0;
        pb::frame(dt);
        draw();
        int64_t spent = now_ms() - t0;
        if (spent < 16) usleep(static_cast<useconds_t>((16 - spent) * 1000));
    }
    save_settings();
    audio::close_stream();
    app->activity->vm->DetachCurrentThread();
}
