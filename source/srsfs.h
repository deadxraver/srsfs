#ifndef _SRSFS_H
#define _SRSFS_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mnt_idmapping.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define MODULE_NAME "srsfs"

#define SRSFS_ROOT_ID 1000
#define SRSFS_FILENAME_LEN 16
#define SRSFS_DIR_CAP 20
#define SRSFS_FSIZE 1024

enum srsfs_fstate {
  UNUSED,
  USED,
};
struct srsfs_dir;

struct shared_data {
  int refcount;
  char data[SRSFS_FSIZE];
  size_t sz;
  struct shared_data* next;
};

struct srsfs_file {
  char name[SRSFS_FILENAME_LEN];
  bool is_dir;
  struct srsfs_dir* ptr;
  enum srsfs_fstate state;
  int id;
  struct shared_data* sd;
};

struct srsfs_dir {
  struct srsfs_file content[SRSFS_DIR_CAP];
  enum srsfs_fstate state;
  int id;
};

static void free_shared(struct shared_data*, bool);

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
);

struct srsfs_file* getfile(int ino);

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset);

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset);

static void init_file(struct srsfs_file* file, const char* name, bool do_alloc);

static void destroy_file(struct srsfs_file* file);

static void init_dir(struct srsfs_dir* dir, const char* name, struct srsfs_file* assoc_file);

static void destroy_dir(struct srsfs_dir* dir);

static struct srsfs_dir* alloc_dir(void);

static bool is_empty(struct srsfs_dir* dir);

static int srsfs_iterate(struct file* filp, struct dir_context* ctx);

static int srsfs_create(struct mnt_idmap*, struct inode*, struct dentry*, umode_t, bool);

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

static int srsfs_mkdir(struct mnt_idmap*, struct inode*, struct dentry*, umode_t);

static int srsfs_rmdir(struct inode*, struct dentry*);

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
