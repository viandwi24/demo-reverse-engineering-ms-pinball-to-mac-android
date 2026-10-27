// Resource dari Pinball.exe milik pengguna: string table (RT_STRING) dan font pesan "PBMSG_FT" (RT_RCDATA).
// Keduanya dibaca saat runtime agar tidak ada konten berhak cipta yang disimpan di repo ini.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace exe_resources {

// Cari Pinball.exe di data_dir (tanpa membedakan huruf besar/kecil) lalu baca resource-nya.
bool load(const std::string& data_dir);

// FUN_01003752 (LoadStringA). Mengembalikan "" jika id tidak ada.
const char* get_string(int id);

// Isi mentah resource RCDATA "PBMSG_FT"; kosong jika tidak ada.
const std::vector<uint8_t>& msg_font();

}  // namespace exe_resources
