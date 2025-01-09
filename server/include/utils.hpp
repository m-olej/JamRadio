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

  void addSongToLibrary(char *file_name,
                        char *file_content); // Interpret buffer

  bool readFully(int fd, char *buffer, size_t size);
};

#endif // UTILS_HPP
