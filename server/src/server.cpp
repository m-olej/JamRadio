#include "json.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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
  std::map<int, sockaddr_in> clients;

public:
  void addClient(int fd, sockaddr_in client_info) { clients[fd] = client_info; }

  void removeClient(int fd) {
    clients.erase(fd);
    close(fd);
  }

  int getActiveListeners() const { return clients.size(); };

  const sockaddr_in &getClient(int fd) {
    auto it = clients.find(fd);

    if (it == clients.end()) {
      throw std::out_of_range("Client not found");
    }
    return it->second;
  }

  const std::map<int, sockaddr_in> &getClients() const { return clients; }
};

class JamRadio {

  int server_fd;
  int epoll_fd;
  ThreadPool threadPool;
  ClientManager clientManager;
  Utils utils;
  bool running;

public:
  JamRadio(int port, size_t thread_count) : threadPool(thread_count) {
    setup(port);
    // Initialize dataStructures
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

  void setup(int port) {
    // Define the server address
    sockaddr_in serverAddress, clientAddress;
    socklen_t client_len = sizeof(clientAddress);

    // Client connection socket
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
      throw std::runtime_error("Server socket creation failed");
    }
    const int one = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Assign epoll descriptor
    epoll_fd = epoll_create1(0);
    epoll_event event = {};
    event.events = EPOLLIN | EPOLLET; // Edge triggered mode
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    // Bind the socket
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(2137);
    serverAddress.sin_family = AF_INET;
    if (bind(server_fd, (sockaddr *)&serverAddress, sizeof(serverAddress)) <
        0) {
      throw std::runtime_error("Failure during socket bind");
      close(server_fd);
    }

    // Listen for client connections
    if (listen(server_fd, 5)) {
      throw std::runtime_error("Listening for connections failed");
      close(server_fd);
    }

    std::cout << "Radio is up! on port " << serverAddress.sin_port << std::endl;
  }

  void acceptConnection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);

    if (client_fd == -1)
      return;
    make_non_blocking(client_fd);

    // Add new client to epoll
    epoll_event event = {}; // Initialize to empty object
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = client_fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

    // Get client connection info
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    std::cout << client_ip << " : " << client_port << " Client connected "
              << std::endl;

    // Add Client to manager
    clientManager.addClient(client_fd, client_addr);
    // Send new client update
    sendUpdate();
  }

  void handleClient(int fd) {
    // Do pretty much everything
    // based on clients[fd]

    sockaddr_in client_info = clientManager.getClient(fd);
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
    case 'f':
      uint32_t filename_size, file_size;

      read(fd, &filename_size, sizeof(filename_size));
      filename_size = ntohl(filename_size);

      std::cout << "filename_size: " << filename_size << std::endl;
      std::vector<char> filename(filename_size);

      read(fd, filename.data(), filename_size);

      std::cout << "filename: " << filename.data() << std::endl;
      read(fd, &file_size, sizeof(file_size));
      file_size = ntohl(file_size);

      std::cout << "file_size: " << file_size << std::endl;
      std::vector<char> file(file_size);

      read(fd, file.data(), file_size);
      std::cout << "file: " << file.data() << std::endl;

      utils.addSongToLibrary(filename.data(), file.data());
      break;
    }

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

    std::string update = updateJson.toString();

    std::cout << update << std::endl;

    for (const auto &client : clientManager.getClients()) {
      send(client.first, update.c_str(), update.size(), 0);
    }
  }

  void start() {
    running = true;

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
          // handleClient(fd);
        }
      }
    }
  }
};

int main(int argc, char *argv[]) {

  try {
    JamRadio server = JamRadio(2137, 1);
    server.start();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
