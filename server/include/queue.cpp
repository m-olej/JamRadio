#include "queue.hpp"
#include "json.hpp"
#include <deque>
#include <fstream>
#include <ios>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

Queue::Queue() {};

Json::Array Queue::getJsonQueue() {
  std::shared_lock<std::shared_mutex> lock(Queue::queue_mutex);

  std::vector<Json> songs;

  for (auto song = Queue::song_queue.begin(); song != Queue::song_queue.end();
       ++song) {
    std::string song_name = song->path.data() + 8;
    songs.push_back(Json(song_name));
  }
  return Json::Array(songs);
}

void Queue::addToQueue(std::string path) {
  std::unique_lock<std::shared_mutex> lock(queue_mutex);
  Song song(path);
  song_queue.push_front(song);
}

std::deque<Queue::Song> Queue::getQueue() {
  std::shared_lock<std::shared_mutex> lock(queue_mutex);
  return Queue::song_queue;
}

bool Queue::isEmpty() {
  std::unique_lock<std::shared_mutex> lock(queue_mutex);
  return Queue::song_queue.empty();
}

std::vector<char> Queue::getChunk() {
  std::shared_lock<std::shared_mutex> lock(Queue::queue_mutex);
  int left = -1;
  int chunk = chunk_size;
  int cursor = song_queue.front().cursor + header_offset;
  std::vector<char> audioChunk(chunk_size);
  if (song_queue.front().file_size - cursor <= chunk_size) {
    left = song_queue.front().file_size - cursor;
    if (left <= 0) {
      song_queue.pop_front();
    } else {
      std::ifstream s1(song_queue.front().path, std::ios::binary);
      if (!s1.is_open()) {
        std::cout << "Inside: " << song_queue.front().cursor << " : "
                  << song_queue.front().file_size << std::endl;
        throw std::runtime_error("Failed during song file open");
      }
      s1.seekg(cursor, std::ios::beg);
      s1.read(audioChunk.data(), left);
      s1.close();
    }
  }
  std::ifstream song(song_queue.front().path, std::ios::binary);
  if (!song.is_open()) {
    std::cout << "Outside: " << song_queue.front().cursor << " : "
              << song_queue.front().file_size << std::endl;
    throw std::runtime_error("Failed during song file open");
  }
  if (left > 0) {
    chunk -= left;
    song.read(audioChunk.data() + left, chunk);
    song.close();
    song_queue.front().cursor += left;
  } else {
    song.seekg(cursor);
    song.read(audioChunk.data(), chunk);
    song.close();
    song_queue.front().cursor += chunk;
  }
  return audioChunk;
}
