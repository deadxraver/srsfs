#include "main.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <unordered_map>

int server_socket = -1;
std::unordered_map<ino_t, Inode> inode_map;

void handle_signal(int) {
  std::cout << "received signal, shutting down..." << std::endl;
  if (server_socket > 0)
    close(server_socket);
  exit(0);
}

void test_map(void) {
  File f;
  f.i_ino = 1001;
  f.name = "unknown";
  std::cout << inode_map[1000].to_string() << std::endl;
  Inode inode(1000, true);
  std::cout << inode.to_string() << std::endl;
  inode_map[1000] = inode;
  std::cout << "inserted new node\n";
  std::cout << inode_map[1000].to_string() << std::endl;
  inode_map[1000].add_file(f);
  std::cout << "inserted new file\n";
  std::cout << inode_map[1000].to_string() << std::endl;
}

int main(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    std::cerr << "Failed to set signal handler" << std::endl;
    return 1;
  }
  test_map();
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    std::cerr << "Could not create socket" << std::endl;
    return 1;
  }
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Could not bind, probably port " << PORT << " is busy or requires root"
              << std::endl;
    close(server_socket);
    return 1;
  }
  if (listen(server_socket, 5) < 0) {
    std::cerr << "Could not listen" << std::endl;
    close(server_socket);
    return 1;
  }
  while (1) {
    int client_socket = accept(server_socket, nullptr, nullptr);
    if (client_socket < 0)
      continue;
    srsfs_request_package reqp;
    srsfs_response_package resp;
    recv(client_socket, &reqp, sizeof(reqp), 0);
    if (reqp.pt == SRSFS_PING) {
      std::cout << "got ping from client" << std::endl;
      resp.pt = SRSFS_PING;
      resp.code = 0;
    } else {
      std::cout << "unknown method or not implemented yet" << std::endl;
      resp.pt = reqp.pt;
      resp.code = -1;
    }
    send(client_socket, &resp, sizeof(resp), 0);
    close(client_socket);
  }
  return 0;
}
