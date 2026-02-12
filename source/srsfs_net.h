#ifndef _SRSFS_NET_H

#define _SRSFS_NET_H

#include <linux/inet.h>
#include <linux/net.h>

#define ADDR "127.0.0.1"

#define PORT 5955
#define NET_DATA_SZ 1024

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

int64_t ping(void);

int64_t send_package(const struct srsfs_request_package* reqp, struct srsfs_response_package* resp);

#endif  // !_SRSFS_NET_H
