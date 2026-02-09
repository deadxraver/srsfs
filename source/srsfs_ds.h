#ifndef _SRSFS_DS_H

#define _SRSFS_DS_H

#include <linux/vmalloc.h>

#define SRSFS_ROOT_ID 1000
#define SRSFS_FSIZE 1024  // 1KB

struct shared_data {  // NOTE: in future versions might be only one node, being reallocated if
                      // needed to expand
  int refcount;
  char data[SRSFS_FSIZE];  // <--
  size_t sz;
  struct shared_data* next;
};

struct srsfs_file {
  char* name;
  int id;
  struct srsfs_file* parent_dir;
  bool is_dir;
  union {
    struct flist* dir_content;
    struct shared_data* data;
  };
};

#endif  // !_SRSFS_DS_H
