#include "json.hpp"
#include "utils.hpp"
#include <arpa/inet.h>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

int make_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class ClientManager {
  std::map<int, sockaddr_in> clients;

public:
  void addClient(int fd, sockaddr_in client_info) { clients[fd] = client_info; }

  void removeClient(int fd) { clients.erase(fd); }

  int getActiveListeners() const { return clients.size(); };

  const std::map<int, sockaddr_in> &getClients() const { return clients; }
};

class JamRadio {

  int server_fd;
  int epoll_fd;
  ClientManager clientManager;
  Utils utils;
  bool running;

public:
  JamRadio(int port) {
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
    event.events = EPOLLIN;
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
    event.events = EPOLLIN;
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

    char buf[50];
    int ret = read(fd, &buf, sizeof(buf));
    if (ret == 0) {
      clientManager.removeClient(fd);
      epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
      std::cout << "Client disconnected" << std::endl;
      sendUpdate();
    }
    write(STDOUT_FILENO, &buf, ret + 1);
  }

  void sendUpdate() {
    Json updateJson;
    // Active listener count
    int activeListeners = clientManager.getActiveListeners();
    std::cout << updateJson.toString() << std::endl;
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
      int n = epoll_wait(epoll_fd, events, 100, -1);
      for (int i = 0; i < n; i++) {
        if (events[i].data.fd == server_fd) {
          acceptConnection();
        } else {
          handleClient(events[i].data.fd);
        }
      }
    }
  }
};

int main(int argc, char *argv[]) {

  try {
    JamRadio server = JamRadio(2137);
    server.start();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
