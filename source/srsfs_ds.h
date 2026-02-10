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

/**
 * Storage for file contents.
 * Array list structure.
 */
struct shared_data {
  int refcount;
  char* data;
  size_t sz;
  size_t capacity;
};

/**
 * File info. Stores name,
 * file type and inode index
 * for data access.
 */
struct srsfs_file {
  char* name;
  bool is_dir;
  int i_ino;
};

/**
 * Field put in inode->i_private.
 * Contains file content if it is
 * a regular file and linked list
 * if is a directory.
 */
struct srsfs_inode_info {
  bool is_dir;
  union {
    struct flist dir_content;
    struct shared_data data;
  };
};

#endif  // !_SRSFS_DS_H
