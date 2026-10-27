// Audio platform: pengganti WaveMix (efek suara) dan MCI sequencer (musik MIDI).
#pragma once

#include <string>

namespace audio {

bool init(const std::string& data_dir);
void shutdown();

// FUN_01006c26: 8 kanal, suara baru memakai kanal yang paling lama dipakai.
void play_wave(const std::string& wav_name);
void set_sound_enabled(bool on);

// FUN_0100afdc / FUN_0100afb9: putar pinball.mid dari awal (diulang terus), atau berhenti.
void set_midi_path(const std::string& path);
void music_play();
void music_stop();
void set_music_enabled(bool on);

}  // namespace audio
