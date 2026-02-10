#include "fino.h"

#include <linux/mm.h>

#include "flist.h"

#define LOG(fmt, ...) pr_info("[srsfs/fino]: " fmt, ##__VA_ARGS__)

struct inode* srsfs_new_inode(
    struct super_block* sb, struct inode* parent, struct srsfs_file* file
) {
  if (sb == NULL && parent == NULL) {
    LOG("srsfs_new_inode: sb & parent are NULL");
    return NULL;
  }
  if (sb == NULL)
    sb = parent->i_sb;
  struct inode* inode = new_inode(sb);
  if (inode == NULL) {
    LOG("srsfs_new_inode: failed to create inode");
    return NULL;
  }
  inode_init_owner(&nop_mnt_idmap, inode, parent, (file->is_dir ? S_IFDIR : S_IFREG) | S_IRWXUGO);
  inode->i_ino = file->i_ino;
  struct srsfs_inode_info* ii =
      (struct srsfs_inode_info*)kvmalloc(sizeof(struct srsfs_inode_info), GFP_KERNEL);
  ii->is_dir = file->is_dir;
  if (ii->is_dir)
    flist_init(&ii->dir_content);
  else
    ii->data = NULL;
  return inode;
}

inline struct inode* srsfs_get_inode(struct super_block* sb, struct srsfs_file* file) {
  return ilookup(sb, file->i_ino);
}
