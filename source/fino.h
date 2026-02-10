#ifndef _FINO_H

#define _FINO_H

#include <linux/fs.h>

#include "flist.h"
#include "srsfs_ds.h"

struct srsfs_inode_info {
  bool is_dir;
  union {
    struct flist dir_content;
    struct shared_data* data;
  };
};

struct inode* srsfs_new_inode(
    struct super_block* sb, struct inode* parent, struct srsfs_file* file
);

struct inode* srsfs_get_inode(struct super_block* sb, struct srsfs_file* file);

#endif  // !_FINO_H
