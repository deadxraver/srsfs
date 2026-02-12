#include "main.hpp"

#include <iostream>

int main(void) {
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
  listen(server_socket, 5);
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
