#include "main.hpp"

#include <csignal>
#include <cstring>
#include <iostream>
#include <unordered_map>

static int server_socket = -1;
static File rootdir;
static ino_t fcnt = SRSFS_ROOT_ID;
static std::unordered_map<ino_t, Inode> inode_map;

#define ALLOC_INO() (fcnt++)

void handle_signal(int) {
  std::cout << "received signal, shutting down..." << std::endl;
  if (server_socket > 0)
    close(server_socket);
  exit(0);
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
  rootdir.i_ino = ALLOC_INO();
  rootdir.name = "srsfs";
  inode_map[rootdir.i_ino] = Inode(rootdir.i_ino, true);
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
    File f;
    std::string name;
    std::string link_name;
    Inode parent_inode;
    Inode target_inode;
    int i = 0;
    ino_t parent_ino;
    ino_t target_ino;
    ino_t i_ino;
    int pos = 0;
    switch (reqp.pt) {
      case SRSFS_PING:
        std::cout << "got ping from client" << std::endl;
        resp.pt = SRSFS_PING;
        resp.code = 0;
        break;
      case SRSFS_ITERATE:
        std::cout << "got iterate request from client" << std::endl;
        resp.pt = SRSFS_ITERATE;
        parent_ino = reqp.iterate.parent_ino;
        pos = reqp.iterate.pos;
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        f = parent_inode.file_at(pos);
        if (f.i_ino < SRSFS_ROOT_ID) {
          resp.iterate.i_ino = 0;
          resp.code = 0;
          break;
        }
        resp.iterate.i_ino = f.i_ino;
        strcpy(resp.iterate.name, f.name.c_str());
        resp.iterate.is_dir = inode_map[f.i_ino].is_dir();
        resp.code = 0;
        break;
      case SRSFS_LOOKUP:
        resp.pt = SRSFS_LOOKUP;
        parent_ino = reqp.lcumr.parent_ino;
        name = std::string(reqp.lcumr.name);
        // TODO: validation
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        i = 0;
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;  // We either found what we need or hit the limit
        }
        if (f.i_ino < SRSFS_ROOT_ID) {
          resp.code = -ENOENT;
          break;
        }
        resp.lcml.i_ino = f.i_ino;
        resp.lcml.sz = inode_map[f.i_ino].sz();
        resp.lcml.st_atim = inode_map[f.i_ino].st_atim();
        resp.lcml.st_mtim = inode_map[f.i_ino].st_mtim();
        resp.code = 0;
        break;
      case SRSFS_CREATE:
        resp.pt = SRSFS_CREATE;
        parent_ino = reqp.lcumr.parent_ino;
        name = std::string(reqp.lcumr.name);
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;
        }
        if (f.name == name) {
          resp.code = -EEXIST;
          break;
        }
        f.i_ino = ALLOC_INO();
        f.name = name;
        inode_map[parent_ino].add_file(f);
        inode_map[f.i_ino] = Inode(f.i_ino, false);
        resp.lcml.i_ino = f.i_ino;
        resp.lcml.sz = inode_map[f.i_ino].sz();
        resp.lcml.st_atim = inode_map[f.i_ino].st_atim();
        resp.lcml.st_mtim = inode_map[f.i_ino].st_mtim();
        resp.code = 0;
        break;
      case SRSFS_UNLINK:
        resp.pt = SRSFS_UNLINK;
        parent_ino = reqp.lcumr.parent_ino;
        name = std::string(reqp.lcumr.name);
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        if (parent_inode.is_dir()) {
          resp.code = -EISDIR;
          break;
        }
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;
        }
        if (f.i_ino < SRSFS_ROOT_ID) {
          resp.code = -ENOENT;
          break;
        }
        i_ino = inode_map[parent_ino].delete_file(f.name);
        inode_map[i_ino].dec_refs();
        resp.code = 0;
        break;
      case SRSFS_MKDIR:
        resp.pt = SRSFS_MKDIR;
        parent_ino = reqp.lcumr.parent_ino;
        name = std::string(reqp.lcumr.name);
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;
        }
        if (f.name == name) {
          resp.code = -EEXIST;
          break;
        }
        f.i_ino = ALLOC_INO();
        f.name = name;
        inode_map[parent_ino].add_file(f);
        inode_map[f.i_ino] = Inode(f.i_ino, true);
        resp.code = 0;
        resp.lcml.i_ino = f.i_ino;
        resp.lcml.sz = inode_map[f.i_ino].sz();
        resp.lcml.st_atim = inode_map[f.i_ino].st_atim();
        resp.lcml.st_mtim = inode_map[f.i_ino].st_mtim();
        resp.code = 0;
        break;
      case SRSFS_RMDIR:
        resp.pt = SRSFS_RMDIR;
        parent_ino = reqp.lcumr.parent_ino;
        name = std::string(reqp.lcumr.name);
        parent_inode = inode_map[parent_ino];
        if (!parent_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        if (!parent_inode.is_dir()) {
          resp.code = -ENOTDIR;
          break;
        }
        // TODO: check if empty
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;
        }
        if (f.i_ino < SRSFS_ROOT_ID) {
          resp.code = -ENOENT;
          break;
        }
        i_ino = inode_map[parent_ino].delete_file(f.name);
        inode_map[i_ino] = Inode();
        resp.code = 0;
        break;
      case SRSFS_LINK:
        resp.pt = SRSFS_LINK;
        parent_ino = reqp.link.parent_ino;
        target_ino = reqp.link.target_ino;
        parent_inode = inode_map[parent_ino];
        target_inode = inode_map[target_ino];
        if (!parent_inode.is_valid() || !target_inode.is_valid()) {
          resp.code = -ENOENT;
          break;
        }
        while (1) {
          f = parent_inode.file_at(i++);
          if (f.i_ino < SRSFS_ROOT_ID || f.name == name)
            break;
        }
        if (f.name == name) {
          resp.code = -EEXIST;
          break;
        }
        f.i_ino = target_ino;
        f.name = link_name;
        inode_map[parent_ino].add_file(f);
        resp.code = 0;
        resp.lcml.i_ino = f.i_ino;
        resp.lcml.sz = inode_map[f.i_ino].sz();
        resp.lcml.st_atim = inode_map[f.i_ino].st_atim();
        resp.lcml.st_mtim = inode_map[f.i_ino].st_mtim();
        resp.code = 0;
        break;
      case SRSFS_READ:
        std::cout << "read not implemented yet\n";
        resp.code = -EPERM;
        // TODO:
        break;
      case SRSFS_WRITE:
        std::cout << "write not implemented yet\n";
        resp.code = -ENOSPC;
        // TODO:
        break;
      default:
        std::cout << "unknown method" << std::endl;
        resp.pt = reqp.pt;
        resp.code = -EINVAL;
        break;
    }
    send(client_socket, &resp, sizeof(resp), 0);
    close(client_socket);
  }
  return 0;
}
