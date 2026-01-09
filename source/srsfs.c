#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "srsfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define SRSFS_ROOT_ID 1000

static struct file_system_type srsfs_fs_type;

static struct inode* srsfs_get_inode(
    struct mnt_idmap* idmap,
    struct super_block* sb,
    const struct inode* dir,
    umode_t mode,
    int i_ino
) {
  struct inode* inode = new_inode(sb);
  if (inode == NULL)
    LOG("failed to create root inode\n");
  else {
    inode_init_owner(idmap, inode, dir, mode);
    inode->i_ino = i_ino;
  }
  return inode;
}

static int srsfs_fill_super(struct super_block* sb, void* data, int silent) {
  // WARN: idmap probably shouldnt be NULL
  struct inode* inode = srsfs_get_inode(NULL, sb, NULL, S_IFDIR, SRSFS_ROOT_ID);

  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    LOG("failed to create root dir\n");
    return -ENOMEM;
  }
  LOG("srsfs filled sb successfully\n");
  return 0;
}

static struct dentry* srsfs_mnt(
    struct file_system_type* fs_type, int flags, const char* token, void* data
) {
  struct dentry* ret = mount_nodev(fs_type, flags, data, srsfs_fill_super);
  if (ret == NULL)
    LOG("failed to mount\n");
  else
    LOG("mounted successfully\n");
  return ret;
}

static void srsfs_kill(struct super_block* sb) {
  LOG("killed sb\n");
}

static void set_fs_params(void) {
  srsfs_fs_type.name = "srsfs";
  srsfs_fs_type.mount = srsfs_mnt;
  srsfs_fs_type.kill_sb = srsfs_kill;
}

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
  set_fs_params();
  register_filesystem(&srsfs_fs_type);
  LOG("SRSFS successfully registered\n");
  return 0;
}

static void __exit srsfs_exit(void) {
  unregister_filesystem(&srsfs_fs_type);
  LOG("SRSFS unregistered successfully\n");
  LOG("SRSFS left the kernel\n");
}

module_init(srsfs_init);
module_exit(srsfs_exit);
