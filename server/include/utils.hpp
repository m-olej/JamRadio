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

  void addSongToLibrary(const char *file_name,
                        const char *file_content); // Interpret buffer
};

#endif // UTILS_HPP
