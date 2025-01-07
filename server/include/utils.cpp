#include "utils.hpp"
#include "json.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <vector>

Utils::Utils() {};

Json::Array Utils::getSongLibrary() {

  std::vector<Json> songs;

  for (const auto &entry :
       std::filesystem::directory_iterator(song_library_path)) {
    std::string song = entry.path().string().erase(0, song_library_path.size());
    songs.push_back(Json(song));
  }

  return Json::Array(songs);
}

void Utils::addSongToLibrary(const char *file_name, const char *file_content) {
  std::ofstream newSong(file_name);

  newSong << file_content;

  return;
}
