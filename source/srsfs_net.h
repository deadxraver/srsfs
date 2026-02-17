#ifndef _SRSFS_NET_H

#define _SRSFS_NET_H

#include <linux/inet.h>
#include <linux/net.h>
#include <linux/time64.h>

#define ADDR "127.0.0.1"

#define SRSFS_ROOT_ID 1000
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
    } iterate;  // iterate
    struct {
      ino_t parent_ino;
      char name[NET_DATA_SZ];
    } lcumr;  // lookup/create/unlink/mkdir/rmdir
    struct {
      ino_t parent_ino;
      ino_t target_ino;
      char name[NET_DATA_SZ];
    } link;  // link
    struct {
      ino_t target_ino;
      size_t len;
      loff_t offset;
      char buffer[NET_DATA_SZ];
    } rw;  // read/write
    struct {
    } ping;  // ping
  };
} __attribute__((packed));

struct srsfs_response_package {
  enum srsfs_package_type pt;
  union {
    struct {
      ino_t i_ino;
      time64_t i_atime_sec;
      time64_t i_mtime_sec;
      size_t sz;
      bool is_dir;
    } lcml;  // lookup/create/mkdir/link
    struct {
      ino_t i_ino;
      char name[NET_DATA_SZ];
      bool is_dir;
    } iterate;  // iterate
    struct {
      size_t bytes_read;
      char buf[NET_DATA_SZ];
    } read;
    struct {
      size_t bytes_written;
    } write;
    struct {
    } urp;  // inlink/rmdir/ping
  };
  int64_t code;
} __attribute__((packed));

int64_t ping(void);

int64_t send_package(const struct srsfs_request_package* reqp, struct srsfs_response_package* resp);

#endif  // !_SRSFS_NET_H
