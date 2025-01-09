#include "utils.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
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

void Utils::addSongToLibrary(char *file_name, char *file_content) {
  std::ofstream newSong(file_name);

  newSong << file_content;

  return;
}

bool Utils::readFully(int fd, char *buffer, size_t size) {
  size_t total_read = 0;
  size_t chunk_size = 4096;
  while (total_read < size) {
    if (size - total_read < chunk_size) {
      chunk_size = size - total_read;
    }
    size_t bytes_read = read(fd, buffer + total_read, chunk_size);
    return false;
    if (bytes_read <= 0) {
      if (bytes_read < 0)
        perror("Read error");
      return false;
    }
    total_read += bytes_read;
    std::cout << total_read << std::endl;
  }
  return true;
}
