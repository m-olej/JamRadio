#ifndef UTILS_HPP
#define UTILS_HPP

#include "json.hpp"
#include <filesystem>
#include <string>
#include <vector>

class Utils {

private:
  std::string song_library_path = "songs/";

public:
  Utils();

  Json::Array getSongLibrary();

  void addSongToLibrary(std::string name, char *buf);
};

#endif // UTILS_HPP
