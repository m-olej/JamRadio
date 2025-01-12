#ifndef QUEUE_HPP
#define QUEUE_HPP

#include "json.hpp"
#include <cstdio>
#include <deque>
#include <fstream>
#include <shared_mutex>
#include <stdexcept>
#include <string>

class Queue {
private:
  mutable std::shared_mutex queue_mutex;
  const int sampling_rate = 48000;
  const int chunk_size = 4096;
  const int header_offset = 0; // standard wav header size
  struct Song {
    std::string path;
    int file_size;
    int cursor; // amount of bytes already sent

    Song(const std::string &file_path)
        : path(file_path), file_size(0), cursor(0) {
      std::ifstream song(file_path, std::ios::binary | std::ios::ate);
      if (!song.is_open()) {
        throw std::runtime_error("Failed to open song file");
      }
      file_size = static_cast<int>(song.tellg());
    }
  };

  std::deque<Song> song_queue;

public:
  Queue();
  // Communication
  Json::Array getJsonQueue();

  void addToQueue(std::string path);

  std::deque<Song> getQueue();

  bool isEmpty();

  // Streaming
  std::vector<char> getChunk();
};

#endif // !QUEUE_HPP
