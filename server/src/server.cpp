#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  // Define the server address
  sockaddr_in serverAddress, clientAddress;
  socklen_t client_len = sizeof(clientAddress);

  // Client connection socket
  int conn_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (conn_sd < 0) {
    perror("Server socket creation failed");
    return 1;
  }

  // Bind the socket
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons(2137);
  serverAddress.sin_family = AF_INET;
  if (bind(conn_sd, (sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
    perror("Failure during socket bind");
    close(conn_sd);
    return 1;
  }

  // Listen for client connections
  if (listen(conn_sd, 5)) {
    perror("Listening for connections failed");
    close(conn_sd);
    return 1;
  }

  std::cout << "Radio is up! on port " << serverAddress.sin_port << std::endl;

  // Accept single client connection
  int client_sd = accept(conn_sd, (sockaddr *)&clientAddress, &client_len);
  if (client_sd < 0) {
    perror("Client connection failed");
    close(conn_sd);
    return 1;
  }

  // Get client connection info
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &clientAddress.sin_addr, client_ip, INET_ADDRSTRLEN);
  int client_port = ntohs(clientAddress.sin_port);

  std::cout << client_ip << " : " << client_port << "Client connected "
            << std::endl;
  char msg_buf[50];
  bool run = true;

  while (run) {
    int ret = read(client_sd, msg_buf, sizeof(msg_buf));
    write(STDOUT_FILENO, msg_buf, ret);
  }

  shutdown(conn_sd, SHUT_WR);
  close(client_sd);
  return 0;
}
