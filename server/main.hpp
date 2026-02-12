#pragma once

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "srsfs_data.hpp"

#define NET_DATA_SZ 1024
#define PORT 5955

enum srsfs_package_type {
  SRSFS_PING = 0,
  SRSFS_ITERATE,
  SRSFS_LOOKUP,
  SRSFS_CREATE,
  SRSFS_UNLINK,
  SRSFS_MKDIR,
  SRSFS_RMDIR,
  SRSFS_LINK,
  SRSFS_READ,
  SRSFS_WRITE,
};

struct srsfs_request_package {
  enum srsfs_package_type pt;
  union {
    struct {
      ino_t parent_ino;
      int pos;
    } iterate;
    struct {
      ino_t parent_ino;
      char name[NET_DATA_SZ];
    } lcumr;
    struct {
      ino_t parent_ino;
      ino_t target_ino;
      char name[NET_DATA_SZ];
    } link;
    struct {
      ino_t target_ino;
      size_t len;
      loff_t offset;
      char buffer[NET_DATA_SZ];
    } rw;
    struct {
    } ping;
  };
};

struct srsfs_response_package {
  enum srsfs_package_type pt;
  union {
    // TODO:
    struct {
    } ping;
  };
  int64_t code;
};
