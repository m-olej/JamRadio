#include "json.hpp"
#include "queue.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <errno.h>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

// Consts
#define CHUNK_SIZE 4096

int make_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Thread pool class
class ThreadPool {
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;

public:
  ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          std::function<void()> task;

          {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty())
              return;
            task = std::move(tasks.front());
            tasks.pop();
          }

          task();
        }
      });
    }
  }

  template <class F> void enqueue(F &&f) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      if (stop)
        throw std::runtime_error("Enqueue on stopped ThreadPool");
      tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
      worker.join();
  }
};

class ClientManager {
  struct Client {
    sockaddr_in client_address;
    int audio_fd;

    // Default constructor for std::map default initilization
    Client() : client_address{}, audio_fd{-1} {};

    Client(sockaddr_in client_address, int audio_fd)
        : client_address(client_address), audio_fd(audio_fd) {};
  };

  mutable std::shared_mutex clients_mutex;

  std::map<int, Client> clients;

public:
  void addClient(int fd, sockaddr_in client_info, int audio_fd) {
    std::unique_lock<std::shared_mutex> lock(clients_mutex);
    clients[fd] = Client(client_info, audio_fd);
  }

  void removeClient(int fd) {
    std::unique_lock<std::shared_mutex> lock(clients_mutex);
    clients.erase(fd);
    close(fd);
  }

  int getActiveListeners() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex);
    return clients.size();
  };

  const Client &getClient(int fd) {
    std::shared_lock<std::shared_mutex> lock(clients_mutex);
    auto it = clients.find(fd);

    if (it == clients.end()) {
      throw std::out_of_range("Client not found");
    }
    return it->second;
  }

  const std::map<int, Client> &getClients() const {
    std::shared_lock<std::shared_mutex> lock(clients_mutex);
    return clients;
  }
};

class JamRadio {

  int server_fd;
  int audio_fd;
  int epoll_fd;
  ThreadPool threadPool;
  ClientManager clientManager;
  Queue queue;
  Utils utils;
  bool running;

public:
  JamRadio(int c_port, int a_port, size_t thread_count)
      : threadPool(thread_count) {
    setup(c_port, a_port);
  }

  ~JamRadio() {
    if (clientManager.getClients().size() > 0) {
      for (const auto &client : clientManager.getClients()) {
        close(client.first);
      }
    }
    shutdown(server_fd, SHUT_WR);
    close(server_fd);
    close(epoll_fd);
  }

  void setup(int c_port, int a_port) {
    // Define the server address
    sockaddr_in serverAddress, audioAddress, clientAddress;
    socklen_t client_len = sizeof(clientAddress);

    // Client connection socket
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
      throw std::runtime_error("Server socket creation failed");
    }
    audio_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (audio_fd < 0) {
      throw std::runtime_error("Audio socket creation failed");
    }
    const int one = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(audio_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    setsockopt(audio_fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));

    // Assign epoll descriptor
    epoll_fd = epoll_create1(0);
    epoll_event event = {};
    event.events = EPOLLIN | EPOLLET; // Edge triggered mode
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    // Bind the socket
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(c_port);
    serverAddress.sin_family = AF_INET;
    if (bind(server_fd, (sockaddr *)&serverAddress, sizeof(serverAddress)) <
        0) {
      throw std::runtime_error("Failure during communication socket bind");
      close(server_fd);
    }
    audioAddress.sin_addr.s_addr = INADDR_ANY;
    audioAddress.sin_port = htons(a_port);
    audioAddress.sin_family = AF_INET;
    if (bind(audio_fd, (sockaddr *)&audioAddress, sizeof(audioAddress)) < 0) {
      perror("Audio bind error: ");
      throw std::runtime_error("Failure during audio socket bind");
      close(audio_fd);
    }

    // Listen for client connections
    if (listen(server_fd, 5)) {
      throw std::runtime_error("Listening for connections failed");
      shutdown(server_fd, SHUT_RDWR);
      close(server_fd);
    }
    if (listen(audio_fd, 5)) {
      throw std::runtime_error("Listening for audio connection failed");
      shutdown(audio_fd, SHUT_RDWR);
      close(audio_fd);
    }

    std::cout << "Radio is up!" << std::endl;
    std::cout << "Communication port: " << ntohs(serverAddress.sin_port)
              << std::endl;
    std::cout << "Audio port: " << ntohs(audioAddress.sin_port) << std::endl;
  }

  void acceptConnection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
      throw std::runtime_error("Client communication connection failure");
    }
    make_non_blocking(client_fd);
    int audio_sd = accept(audio_fd, nullptr, nullptr);
    if (audio_sd == -1) {
      throw std::runtime_error("Client audio communication failure");
    }
    make_non_blocking(audio_sd);

    // Add new client to epoll
    epoll_event event = {}; // Initialize to empty object
    event.events =
        EPOLLIN | EPOLLET |
        EPOLLONESHOT; // Trigger once when new data comes from client socket
    event.data.fd = client_fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

    // Get client connection info
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    std::cout << client_ip << " : " << client_port << " Client connected "
              << std::endl;

    // Add Client to manager
    clientManager.addClient(client_fd, client_addr, audio_sd);
    // Send new client update
    sendUpdate();
  }

  void handleClient(int fd) {
    // Do pretty much everything
    // based on clients[fd]

    sockaddr_in client_info = clientManager.getClient(fd).client_address;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_info.sin_addr, client_ip, INET_ADDRSTRLEN);
    // Debug
    std::cout << "From client: " << client_ip << std::endl;

    char signature;

    int ret = read(fd, &signature, sizeof(signature));
    if (ret <= 0) {
      if (ret == 0) {
        clientManager.removeClient(fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        std::cout << "Client disconnected" << std::endl;
        sendUpdate();
      }
      return;
    }

    // Handle multiple types of communication (buf[0] - signature)
    switch (signature) {
    case 'f': {
      std::cout << "Receiving file" << std::endl;
      uint32_t filename_size, file_size;
      const int chunk_size = 4096;
      read(fd, &filename_size, sizeof(filename_size));
      filename_size = ntohl(filename_size);

      std::cout << "filename_size: " << filename_size << std::endl;
      std::vector<char> filename(filename_size);

      read(fd, filename.data(), filename_size);

      std::cout << "filename: " << filename.data() << std::endl;
      read(fd, &file_size, sizeof(file_size));
      file_size = ntohl(file_size);

      std::cout << "file_size: " << file_size << std::endl;

      std::ofstream newSong(filename.data(), std::ios::binary);
      if (!newSong.is_open()) {
        throw std::runtime_error("Song file writing error");
      };

      uint32_t bytes_read;
      uint32_t total = 0;
      char audio_buffer[CHUNK_SIZE]{};
      while ((bytes_read = read(fd, audio_buffer, CHUNK_SIZE))) {
        newSong.write(audio_buffer, bytes_read);
        std::cout << "Written " << bytes_read << " bytes" << std::endl;
        total += bytes_read;
        std::cout << "Total bytes read: " << total << std::endl;
        if (total >= file_size) {
          std::cout << filename.data() << " added to the song library"
                    << std::endl;
          break;
        }
      }

      break;
    }
    case 'q': {
      std::cout << "Adding to queue" << std::endl;
      uint32_t songname_size;
      read(fd, &songname_size, sizeof(songname_size));
      songname_size = ntohl(songname_size);

      std::cout << "Song name size: " << songname_size << std::endl;

      std::vector<char> songname(songname_size);
      read(fd, songname.data(), songname_size);

      std::cout << "Song name: " << songname.data() << std::endl;

      queue.addToQueue(songname.data());
      break;
    }
    }

    // Reactivate clients socket events
    epoll_event event = {}; // Initialize to empty object
    event.events =
        EPOLLIN | EPOLLET |
        EPOLLONESHOT; // Trigger once when new data comes from client socket
    event.data.fd = fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);

    // Each client communication changes the server state
    // Send updated server state to each client
    sendUpdate();
  }

  void sendUpdate() {
    Json updateJson;
    // Active listener count
    int activeListeners = clientManager.getActiveListeners();
    updateJson["active_listeners"] = Json(activeListeners);
    // Server song library
    updateJson["song_library"] = utils.getSongLibrary();
    updateJson["song_queue"] = queue.getJsonQueue();

    std::string update = updateJson.toString();

    std::cout << update << std::endl;

    for (const auto &client : clientManager.getClients()) {
      send(client.first, update.c_str(), update.size(), 0);
    }
  }

  void start() {
    running = true;

    std::thread audio_stream(&JamRadio::streamCast, this);
    audio_stream.detach();
    epoll_event events[100];
    while (running) {
      std::cout << "waiting" << std::endl;
      int n = epoll_wait(epoll_fd, events, 100, -1);
      for (int i = 0; i < n; i++) {
        if (events[i].data.fd == server_fd) {
          acceptConnection();
        } else {
          int fd = events[i].data.fd;
          threadPool.enqueue([this, fd]() { handleClient(fd); });
        }
      }
    }
  }

  void streamCast() {
    while (running) {
      if (!queue.isEmpty()) {
        std::vector<char> chunk = queue.getChunk();
        std::cout << chunk.data();
        for (const auto &client : clientManager.getClients()) {
          if (send(client.second.audio_fd, chunk.data(), chunk.size(), 0) < 0) {
            perror("Audio stream error: ");
          };
          break;
        }
      }
    }
  }
};

int main(int argc, char *argv[]) {

  try {
    JamRadio server = JamRadio(atoi(argv[1]), atoi(argv[2]), 4);
    server.start();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
