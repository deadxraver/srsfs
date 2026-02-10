#ifndef _SRSFS_DS_H

#define _SRSFS_DS_H

#include <linux/fs.h>
#include <linux/types.h>

#define SRSFS_ROOT_ID 1000
#define SRSFS_FSIZE 1024  // 1KB

struct srsfs_file;

/**
 * double-linked round list storing srsfs files.
 * Each node represents single file.
 * Head node should not contain useful info.
 */
struct flist {  // NOTE: should be allocated/freed using kvmalloc/kvfree respectively
  struct srsfs_file* content;
  struct flist* next;
  struct flist* prev;
};

struct shared_data {  // NOTE: in future versions might be only one node, being reallocated if
                      // needed to expand
  int refcount;
  char data[SRSFS_FSIZE];  // <--
  size_t sz;
  struct shared_data* next;
};

struct srsfs_file {
  char* name;
  bool is_dir;
  int i_ino;
};

struct srsfs_inode_info {
  bool is_dir;
  union {
    struct flist dir_content;
    struct shared_data* data;
  };
};

#endif  // !_SRSFS_DS_H
