#ifndef _SRSFS_H
#define _SRSFS_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mnt_idmapping.h>
#include <linux/module.h>

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define MODULE_NAME "srsfs"

#define SRSFS_ROOT_ID 1000
#define SRSFS_FILENAME_LEN 16
#define SRSFS_DIR_CAP 20

enum srsfs_fstate {
  UNUSED,
  USED,
};
struct srsfs_dir;

struct srsfs_file {
  char name[SRSFS_FILENAME_LEN];
  bool is_dir;
  struct srsfs_dir* ptr;
  enum srsfs_fstate state;
  int id;
};

struct srsfs_dir {
  char name[SRSFS_FILENAME_LEN];
  struct srsfs_file content[SRSFS_DIR_CAP];
  enum srsfs_fstate state;
};

static void init_file(struct srsfs_file* file, const char* name);

static int srsfs_iterate(struct file* filp, struct dir_context* ctx);

static int srsfs_create(struct mnt_idmap*, struct inode*, struct dentry*, umode_t, bool);

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

static struct dentry* srsfs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
);

static struct inode* srsfs_get_inode(
    struct mnt_idmap*, struct super_block*, const struct inode*, umode_t, int
);

static int srsfs_fill_super(struct super_block* sb, void* data, int silent);

static struct dentry* srsfs_mnt(
    struct file_system_type* fs_type, int flags, const char* token, void* data
);

static void srsfs_kill(struct super_block* sb);

static void set_fs_params(void);

#endif  // !_SRSFS_H
