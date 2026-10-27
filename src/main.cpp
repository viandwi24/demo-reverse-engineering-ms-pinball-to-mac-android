// App Mac: meja Space Cadet dengan fisika (tahap 2).
//
//   pinball [folder_data]                 main interaktif (F2 = game baru, F3 = pause, Z / tanda / = flipper,
//                                         spasi = plunger, X . panah-atas = dorong meja)
//   pinball [folder_data] --demo <folder>  jalankan skenario uji dan simpan screenshot
#include <SDL.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "dat_file.h"
#include "exe_resources.h"
#include "platform/audio.h"
#include "engine/ball.h"
#include "engine/objects.h"
#include "engine/render.h"
#include "engine/table.h"

namespace {

// Pengganti registry "Software\\Microsoft\\Plus!\\Pinball\\SpaceCadet": file teks di Application Support.
std::string settings_path() {
    char* pref = SDL_GetPrefPath("Microsoft", "SpaceCadetPinball");
    std::string p = pref ? std::string(pref) + "settings.txt" : "settings.txt";
    SDL_free(pref);
    return p;
}

struct Settings {
    int players = 1;
    int sounds = 1;
    int music = 1;
};

Settings load_settings() {
    Settings st;
    if (FILE* f = std::fopen(settings_path().c_str(), "r")) {
        char key[64];
        char val[64];
        while (std::fscanf(f, "%63s %63[^\n]", key, val) == 2) {
            std::string k = key;
            if (k == "Players") st.players = std::atoi(val);
            else if (k == "Sounds") st.sounds = std::atoi(val);
            else if (k == "Music") st.music = std::atoi(val);
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
    return st;
}

void save_settings(const Settings& st) {
    if (FILE* f = std::fopen(settings_path().c_str(), "w")) {
        std::fprintf(f, "Players %d\nSounds %d\nMusic %d\n", st.players, st.sounds, st.music);
        for (int i = 0; i < 5; ++i)
            std::fprintf(f, "HighScore%d %d %s\n", i, highscore::g_table[i].score, highscore::g_table[i].name);
        std::fclose(f);
    }
}

std::array<uint32_t, 256> load_palette(const DatFile& dat) {
    std::array<uint32_t, 256> argb{};
    const DatEntry* pal = dat.find("background", EntryType::Palette);
    for (int i = 0; i < 256; ++i) {
        const uint8_t* c = &pal->raw[i * 4];
        argb[i] = 0xff000000u | (c[2] << 16) | (c[1] << 8) | c[0];
    }
    return argb;
}


bool save_bmp(const char* path, const std::vector<uint32_t>& argb, int w, int h) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32_t*>(argb.data()), w, h, 32, w * 4,
                                                        SDL_PIXELFORMAT_ARGB8888);
    int rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc == 0;
}

// SDL keycode -> kode virtual-key Windows yang dipakai exe asli.
int to_vk(SDL_Keycode k) {
    if (k >= SDLK_a && k <= SDLK_z) return 'A' + (k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (k - SDLK_0);
    switch (k) {
    case SDLK_SPACE: return 32;
    case SDLK_SLASH: return 191;
    case SDLK_PERIOD: return 190;
    case SDLK_COMMA: return 188;
    case SDLK_UP: return 38;
    case SDLK_DOWN: return 40;
    case SDLK_LEFT: return 37;
    case SDLK_RIGHT: return 39;
    case SDLK_RETURN: return 13;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 16;
    case SDLK_LCTRL: case SDLK_RCTRL: return 17;
    default: return 0;
    }
}

// Geser frame sesuai nudge (render offset), area kosong hitam.
void present_frame(const Bitmap8& frame, const std::array<uint32_t, 256>& pal, std::vector<uint32_t>& out) {
    int ox, oy;
    pb::render_offset(ox, oy);
    out.assign(static_cast<size_t>(frame.width) * frame.height, 0xff000000u);
    for (int y = 0; y < frame.height; ++y)
        for (int x = 0; x < frame.width; ++x) {
            int sx = x - ox, sy = y - oy;
            if (sx >= 0 && sy >= 0 && sx < frame.width && sy < frame.height)
                out[y * frame.width + x] = pal[frame.pixels[sy * frame.stride + sx]];
        }
}

int run_demo(const std::array<uint32_t, 256>& pal, const std::string& out_dir) {
    // Skenario: bola di plunger, tarik 1 detik, lepas, lalu flipper ditekan berkala.
    std::vector<uint32_t> argb;
    const int step_ms = 10;
    int shot = 0;
    for (int t = 0; t <= 8000; t += step_ms) {
        if (t == 2000) pb::keydown(32);
        if (t == 3500) pb::keyup(32);
        for (int f = 5000; f <= 8000; f += 700) {
            if (t == f) pb::keydown(90), pb::keydown(191);
            if (t == f + 250) pb::keyup(90), pb::keyup(191);
        }
        pb::frame(step_ms);
        if (t % 100 == 0) {
            TBall* ball = pb::g_main_table->ball_list[0];
            std::printf("t=%5d ms  bola=(%.3f, %.3f) v=%.3f aktif=%d\n", t, ball->position.x, ball->position.y,
                        ball->speed, ball->active_flag);
            const Bitmap8& frame = render::frame();
            present_frame(frame, pal, argb);
            char path[512];
            std::snprintf(path, sizeof(path), "%s/frame_%03d.bmp", out_dir.c_str(), shot++);
            save_bmp(path, argb, frame.width, frame.height);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "3dPinballWindows";
    const char* demo_dir = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0 && i + 1 < argc)
            demo_dir = argv[++i];
        else
            data_dir = argv[i];
    }

    auto dat = DatFile::load(data_dir + "/PINBALL.DAT");
    if (!dat) {
        std::fprintf(stderr, "Gagal memuat %s/PINBALL.DAT — lihat 3dPinballWindows/README.md\n", data_dir.c_str());
        return 1;
    }
    if (!exe_resources::load(data_dir)) {
        std::fprintf(stderr, "Gagal membaca resource %s/Pinball.exe — lihat 3dPinballWindows/README.md\n", data_dir.c_str());
        return 1;
    }
    pb::create_table(dat.get(), data_dir);
    auto pal = load_palette(*dat);
    std::printf("Meja dibuat: %zu komponen\n", pb::g_main_table->component_list.size());

    if (!demo_dir && audio::init(data_dir)) {
        audio::set_midi_path(data_dir + "/PINBALL.MID");
        loader::set_sound_player([](const std::string& wav) { audio::play_wave(wav); });
        pb::g_music_hook = [](bool play) { play ? audio::music_play() : audio::music_stop(); };
    }
    Settings settings = load_settings();
    pb::g_players = settings.players;
    audio::set_sound_enabled(settings.sounds != 0);
    audio::set_music_enabled(settings.music != 0);
    pb::replay_level(0);  // WinMain: FUN_010153ae(0) — langsung game baru
    if (demo_dir) return run_demo(pal, demo_dir);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    const Bitmap8& first = render::vscreen();
    int w = first.width, h = first.height;
    SDL_Window* window = SDL_CreateWindow("3D Pinball - Space Cadet", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          w * 2, h * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, w, h);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);

    std::vector<uint32_t> argb;
    uint32_t last = SDL_GetTicks();
    for (bool running = true; running;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;
                int vk = to_vk(k);
                if (e.type == SDL_KEYDOWN) {
                    if (k == SDLK_F2) pb::replay_level(0);
                    else if (k == SDLK_F3) pb::pause_continue();
                    else if (k >= SDLK_F5 && k <= SDLK_F8) {  // F5-F8: jumlah pemain 1-4 (menu Options > Players)
                        settings.players = k - SDLK_F5 + 1;
                        pb::g_players = settings.players;
                    } else if (k == SDLK_F9) {  // Options > Sounds
                        settings.sounds = !settings.sounds;
                        audio::set_sound_enabled(settings.sounds != 0);
                    } else if (k == SDLK_F10) {  // Options > Music
                        settings.music = !settings.music;
                        audio::set_music_enabled(settings.music != 0);
                        if (settings.music) audio::music_play();
                    }
                    else if (vk) pb::keydown(vk);
                } else if (vk) {
                    pb::keyup(vk);
                }
            }
        }
        uint32_t now = SDL_GetTicks();
        pb::frame(static_cast<int>(now - last));
        last = now;

        present_frame(render::frame(), pal, argb);
        SDL_UpdateTexture(texture, nullptr, argb.data(), w * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    save_settings(settings);
    audio::shutdown();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
