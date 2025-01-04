#include "utils.hpp"
#include "json.hpp"
#include <filesystem>

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
