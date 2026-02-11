#ifndef _SRSFS_H
#define _SRSFS_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mnt_idmapping.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "srsfs_ds.h"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define MODULE_NAME "srsfs"

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
);

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset);

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset);

static int srsfs_iterate(struct file* filp, struct dir_context* ctx);

static int srsfs_create(struct mnt_idmap*, struct inode*, struct dentry*, umode_t, bool);

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry);

static int srsfs_mkdir(struct mnt_idmap*, struct inode*, struct dentry*, umode_t);

static int srsfs_rmdir(struct inode*, struct dentry*);

static struct dentry* srsfs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
);

static int srsfs_fill_super(struct super_block* sb, void* data, int silent);

static struct dentry* srsfs_mnt(
    struct file_system_type* fs_type, int flags, const char* token, void* data
);

static void srsfs_kill(struct super_block* sb);

#endif  // !_SRSFS_H
