// Musik: pinball.mid lewat MusicPlayer AudioToolbox (synth DLS bawaan macOS, offline).
// Asli: MCI sequencer, diputar ulang dari awal setiap selesai (MM_MCINOTIFY).
#include <AudioToolbox/AudioToolbox.h>

#include <atomic>
#include <thread>

#include "audio.h"

namespace audio {
namespace {
MusicPlayer g_player = nullptr;
MusicSequence g_sequence = nullptr;
MusicTimeStamp g_length = 0;
bool g_music_on = true;
std::atomic<bool> g_looping{false};
std::thread g_loop_thread;
std::string g_midi_path;

void ensure_loaded() {
    if (g_sequence) return;
    NewMusicSequence(&g_sequence);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(g_midi_path.c_str()),
                                                           static_cast<CFIndex>(g_midi_path.size()), false);
    if (MusicSequenceFileLoad(g_sequence, url, kMusicSequenceFile_MIDIType, 0) != noErr) {
        DisposeMusicSequence(g_sequence);
        g_sequence = nullptr;
    }
    CFRelease(url);
    if (!g_sequence) return;
    UInt32 tracks = 0;
    MusicSequenceGetTrackCount(g_sequence, &tracks);
    for (UInt32 i = 0; i < tracks; ++i) {
        MusicTrack track;
        MusicTimeStamp len = 0;
        UInt32 size = sizeof(len);
        MusicSequenceGetIndTrack(g_sequence, i, &track);
        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &len, &size);
        if (len > g_length) g_length = len;
    }
    NewMusicPlayer(&g_player);
    MusicPlayerSetSequence(g_player, g_sequence);
    MusicPlayerPreroll(g_player);
}
}  // namespace

void set_midi_path(const std::string& path) { g_midi_path = path; }

void music_play() {
    if (!g_music_on) return;
    music_stop();
    ensure_loaded();
    if (!g_player) return;
    MusicPlayerSetTime(g_player, 0);
    MusicPlayerStart(g_player);
    g_looping = true;
    g_loop_thread = std::thread([] {
        while (g_looping) {
            MusicTimeStamp now = 0;
            MusicPlayerGetTime(g_player, &now);
            if (now >= g_length) {
                MusicPlayerSetTime(g_player, 0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

void music_stop() {
    g_looping = false;
    if (g_loop_thread.joinable()) g_loop_thread.join();
    if (g_player) MusicPlayerStop(g_player);
}

void set_music_enabled(bool on) {
    g_music_on = on;
    if (!on) music_stop();
}

}  // namespace audio
