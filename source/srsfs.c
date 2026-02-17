#include "srsfs.h"

#include "list.h"
#include "srsfs_dbg_logs.h"
#include "srsfs_futil.h"
#include "srsfs_net.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

static struct srsfs_file rootdir;
static int fcnt = 0;
static struct inode* root_inode = NULL;

#define ALLOC_ID() (SRSFS_ROOT_ID + fcnt++)

static struct file_system_type srsfs_fs_type = {
    .name = "srsfs",
    .mount = srsfs_mnt,
    .kill_sb = srsfs_kill,
};

static struct inode_operations srsfs_inode_ops = {
    .lookup = srsfs_lookup,
    .create = srsfs_create,
    .unlink = srsfs_unlink,
    .mkdir = srsfs_mkdir,
    .rmdir = srsfs_rmdir,
    .link = srsfs_link,
};

struct file_operations srsfs_dir_ops = {
    .iterate_shared = srsfs_iterate,
};

struct file_operations srsfs_file_ops = {
    .read = srsfs_read,
    .write = srsfs_write,
};

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_LINK;
  const char* name = new_dentry->d_name.name;
  strcpy(reqp.link.name, name);
  reqp.link.parent_ino = parent_dir->i_ino;
  reqp.link.target_ino = d_inode(old_dentry)->i_ino;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("send finished with error %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("link: recieved error code %ld", resp.code);
    return resp.code;
  }
  struct inode* inode = new_inode(parent_dir->i_sb);
  inode->i_ino = resp.lcml.i_ino;
  inode->i_atime_sec = resp.lcml.i_atime_sec;
  inode->i_mtime_sec = resp.lcml.i_mtime_sec;
  inode->i_size = resp.lcml.sz;
  inode->i_fop = &srsfs_file_ops;
  inode->i_op = &srsfs_inode_ops;
  inode_init_owner(&nop_mnt_idmap, inode, parent_dir, S_IFREG | S_IRWXUGO);
  d_add(new_dentry, inode);
  LOG("Success.");
  return 0;
}

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_READ;
  struct inode* inode = filp->f_inode;
  reqp.rw.target_ino = inode->i_ino;
  reqp.rw.len = min(len, NET_DATA_SZ);
  reqp.rw.offset = *offset;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("read: send failed: %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("read: server responded with error code %ld", resp.code);
    return resp.code;
  }
  if (copy_to_user(buffer, resp.read.buf, resp.read.bytes_read))
    return -EFAULT;
  *offset += resp.read.bytes_read;
  return resp.read.bytes_read;
}

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_WRITE;
  struct inode* inode = filp->f_inode;
  reqp.rw.target_ino = inode->i_ino;
  reqp.rw.len = len;
  reqp.rw.offset = *offset;
  if (copy_from_user(reqp.rw.buffer, buffer, min(len, NET_DATA_SZ)))
    return -EFAULT;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("write: send failed: %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("write: server responded with error code %ld", resp.code);
    return resp.code;
  }
  *offset += resp.write.bytes_written;
  return resp.write.bytes_written;
}

static int srsfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_ITERATE;
  reqp.iterate.parent_ino = d_inode(dentry)->i_ino;
  if (!dir_emit_dots(filp, ctx))
    return 0;
  while (1) {
    reqp.iterate.pos = ctx->pos - 2;
    int64_t err = send_package(&reqp, &resp);
    if (err < 0) {
      LOG("lookup: send failed");
      return -EAGAIN;
    }
    if (resp.code) {
      LOG("server returned %ld", resp.code);
      return resp.code;
    }
    if (resp.iterate.i_ino < SRSFS_ROOT_ID)
      return 0;
    if (!dir_emit(
            ctx,
            resp.iterate.name,
            strlen(resp.iterate.name),
            resp.iterate.i_ino,
            (resp.iterate.is_dir ? DT_DIR : DT_REG)
        ))
      return 0;
    ctx->pos++;
  }
}

static struct dentry* srsfs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  const char* name = child_dentry->d_name.name;
  reqp.pt = SRSFS_LOOKUP;
  strcpy(reqp.lcumr.name, name);
  reqp.lcumr.parent_ino = parent_inode->i_ino;
  int64_t res = send_package(&reqp, &resp);
  if (res) {
    LOG("srsfs_lookup: send error: %ld", res);
    return ERR_PTR(res);
  }
  if (resp.code) {
    LOG("srsfs_lookup: server responded with error %ld", resp.code);
    LOG("srsfs_lookup: tried to find: %s", name);
    return NULL;
  }
  struct inode* inode = NULL;  // ilookup(parent_inode->i_sb, resp.lcml.i_ino);
  if (inode == NULL) {
    inode = new_inode(parent_inode->i_sb);
    inode_init_owner(
        &nop_mnt_idmap, inode, parent_inode, (resp.lcml.is_dir ? S_IFDIR : S_IFREG) | S_IRWXUGO
    );
    inode->i_op = &srsfs_inode_ops;
    if (resp.lcml.is_dir) {
      inode->i_fop = &srsfs_dir_ops;
      set_nlink(inode, 2);
    } else
      inode->i_fop = &srsfs_file_ops;
  }
  inode->i_ino = resp.lcml.i_ino;
  inode->i_atime_sec = resp.lcml.i_atime_sec;
  inode->i_mtime_sec = resp.lcml.i_mtime_sec;
  inode->i_size = resp.lcml.sz;
  d_add(child_dentry, inode);
  return NULL;
}

static int srsfs_create(
    struct mnt_idmap* idmap,
    struct inode* parent_inode,
    struct dentry* child_dentry,
    umode_t mode,
    bool b
) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_CREATE;
  const char* name = child_dentry->d_name.name;
  strcpy(reqp.lcumr.name, name);
  reqp.lcumr.parent_ino = parent_inode->i_ino;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("send finished with error %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("recieved error code %ld", resp.code);
    return resp.code;
  }
  struct inode* inode = new_inode(parent_inode->i_sb);
  inode->i_ino = resp.lcml.i_ino;
  inode->i_atime_sec = resp.lcml.i_atime_sec;
  inode->i_mtime_sec = resp.lcml.i_mtime_sec;
  inode->i_size = resp.lcml.sz;
  inode->i_fop = &srsfs_file_ops;
  inode->i_op = &srsfs_inode_ops;
  inode_init_owner(idmap, inode, parent_inode, S_IFREG | S_IRWXUGO);
  d_add(child_dentry, inode);
  LOG("Success.");
  return 0;
}

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_UNLINK;
  const char* name = child_dentry->d_name.name;
  strcpy(reqp.lcumr.name, name);
  reqp.lcumr.parent_ino = parent_inode->i_ino;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("send finished with error %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("recieved error code %ld", resp.code);
    return resp.code;
  }
  LOG("Success.");
  return 0;
}

static int srsfs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_MKDIR;
  const char* name = child_dentry->d_name.name;
  strcpy(reqp.lcumr.name, name);
  reqp.lcumr.parent_ino = parent_inode->i_ino;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("mkdir: send finished with error %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("mkdir: recieved error code %ld", resp.code);
    return resp.code;
  }
  struct inode* inode = new_inode(parent_inode->i_sb);
  inode->i_ino = resp.lcml.i_ino;
  inode->i_atime_sec = resp.lcml.i_atime_sec;
  inode->i_mtime_sec = resp.lcml.i_mtime_sec;
  inode->i_size = resp.lcml.sz;
  inode->i_fop = &srsfs_dir_ops;
  inode->i_op = &srsfs_inode_ops;
  inode_init_owner(idmap, inode, parent_inode, S_IFDIR | S_IRWXUGO);
  d_add(child_dentry, inode);
  LOG("Success.");
  return 0;
}

static int srsfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  struct srsfs_request_package reqp;
  struct srsfs_response_package resp;
  reqp.pt = SRSFS_RMDIR;
  const char* name = child_dentry->d_name.name;
  strcpy(reqp.lcumr.name, name);
  reqp.lcumr.parent_ino = parent_inode->i_ino;
  int64_t err = send_package(&reqp, &resp);
  if (err) {
    LOG("send finished with error %ld", err);
    return -EAGAIN;
  }
  if (resp.code) {
    LOG("recieved error code %ld", resp.code);
    return resp.code;
  }
  LOG("Success.");
  return 0;
}

static int srsfs_fill_super(struct super_block* sb, void* data, int silent) {
  int64_t err = ping();
  if (err) {
    LOG("mount: could not ping server, %ld", err);
    return -EAGAIN;
  } else
    LOG("server ping returned OK");
  init_dir(&rootdir, "srsfs", SRSFS_ROOT_ID);
  root_inode = new_inode(sb);
  root_inode->i_ino = SRSFS_ROOT_ID;
  root_inode->i_size = PAGE_SIZE;
  root_inode->i_op = &srsfs_inode_ops;
  root_inode->i_fop = &srsfs_dir_ops;
  inode_init_owner(&nop_mnt_idmap, root_inode, NULL, S_IFDIR | S_IRWXUGO);
  sb->s_root = d_make_root(root_inode);
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

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
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
