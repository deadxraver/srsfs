#include "srsfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

static struct file_system_type srsfs_fs_type;

static struct srsfs_dir* rootdir;
static struct srsfs_dir dirs[100];
static int fcnt = 0;

static int srsfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = d_inode(dentry);
  struct inode* parent_inode;
  ino_t ino = inode->i_ino;
  ino_t parent_ino;

  if (ctx->pos == 0) {
    if (!dir_emit(ctx, ".", 1, ino, DT_DIR))
      return 0;
    ctx->pos++;
  }

  if (ctx->pos == 1) {
    parent_inode = d_inode(dentry->d_parent);
    parent_ino = parent_inode->i_ino;
    if (!dir_emit(ctx, "..", 2, parent_ino, DT_DIR))
      return 0;
    ctx->pos++;
  }

  if (ino == SRSFS_ROOT_ID) {
    for (int i = ctx->pos - 2; i < SRSFS_DIR_CAP;
         ++i) {  // there might be a problem with this solution, need to be tested
      if (rootdir->content[i].state == UNUSED)
        continue;
      if (!dir_emit(
              ctx,
              rootdir->content[i].name,
              strlen(rootdir->content[i].name),
              rootdir->content[i].id,
              (rootdir->content[i].is_dir ? DT_DIR : DT_REG)
          ))
        return 0;
      ctx->pos++;
    }
  }

  return 0;
}

struct file_operations srsfs_dir_ops = {
    .iterate_shared = srsfs_iterate,
};

static void init_file(struct srsfs_file* file, const char* name) {
  if (file->state == USED)
    return;
  file->state = USED;
  strncpy(file->name, name, SRSFS_FILENAME_LEN);
  file->name[SRSFS_FILENAME_LEN - 1] = 0;
  file->is_dir = 0;
  file->id = SRSFS_ROOT_ID + ++fcnt;
}

static void destroy_file(struct srsfs_file* file) {
  file->state = UNUSED;
  memset(file->name, 0, SRSFS_FILENAME_LEN);
  if (file->is_dir)
    destroy_dir(file->ptr);
  file->is_dir = 0;
  file->id = 0;
}

static void init_dir(struct srsfs_dir* dir, const char* name, struct srsfs_file* assoc_file) {
  if (assoc_file) {
    init_file(assoc_file, name);
    assoc_file->is_dir = 1;
    assoc_file->ptr = dir;
  }
  dir->state = USED;
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    destroy_file(dir->content + i);
  }
}

static void destroy_dir(struct srsfs_dir* dir) {
  dir->state = UNUSED;
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    destroy_file(dir->content + i);
  }
}

static struct srsfs_dir* alloc_dir(void) {
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED) {
      return dirs + i;
    }
  }
  return NULL;
}

static void free_dir(struct srsfs_dir* dir) {
  dir->state = UNUSED;
}

static bool is_empty(struct srsfs_dir* dir) {
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    if (dir->content[i].state == USED)
      return 0;
  }
  return 1;
}

static struct dentry* srsfs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
) {
  ino_t root = parent_inode->i_ino;
  const char* name = child_dentry->d_name.name;
  if (root == SRSFS_ROOT_ID) {
    for (int i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (!strcmp(name, rootdir->content[i].name)) {
        struct inode* inode = srsfs_get_inode(
            &nop_mnt_idmap,
            parent_inode->i_sb,
            NULL,
            rootdir->content[i].is_dir ? S_IFDIR : S_IFREG,
            rootdir->content[i].id
        );
        d_add(child_dentry, inode);
        return NULL;
      }
    }
  }
  return NULL;
}

static struct inode_operations srsfs_inode_ops = {
    .lookup = srsfs_lookup,
    .create = srsfs_create,
    .unlink = srsfs_unlink,
    .mkdir = srsfs_mkdir,
    .rmdir = srsfs_rmdir,
};

static int srsfs_create(
    struct mnt_idmap* idmap,
    struct inode* parent_inode,
    struct dentry* child_dentry,
    umode_t mode,
    bool b
) {
  ino_t root = parent_inode->i_ino;
  const char* name = child_dentry->d_name.name;
  if (root == SRSFS_ROOT_ID) {
    for (int i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (strcmp(rootdir->content[i].name, name) == 0)
        return -EEXIST;
    }
    for (int i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (rootdir->content[i].state == UNUSED) {
        init_file(rootdir->content + i, name);
        struct inode* inode = srsfs_get_inode(
            idmap, parent_inode->i_sb, NULL, S_IFREG | S_IRWXUGO, rootdir->content[i].id
        );
        inode->i_op = &srsfs_inode_ops;
        inode->i_fop = NULL;
        d_add(child_dentry, inode);
        return 0;
      }
    }
  }
  return 0;
}

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  if (root == SRSFS_ROOT_ID) {
    for (int i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (strcmp(rootdir->content[i].name, name) == 0) {
        destroy_file(rootdir->content + i);
        return 0;
      }
    }
    return -ENOENT;
  }
  return 0;
}

static int srsfs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  if (root == SRSFS_ROOT_ID) {
    for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (rootdir->content[i].state == UNUSED) {
        struct srsfs_dir* dir = alloc_dir();
        if (dir == NULL)
          return -ENOMEM;
        init_dir(dir, name, rootdir->content + i);
        return 0;
      }
    }
    return -ENOMEM;
  }
  return 0;
}

static int srsfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  if (root == SRSFS_ROOT_ID) {
    for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
      if (strcmp(name, rootdir->content[i].name) == 0) {
        if (!rootdir->content[i].is_dir)
          return 1;
        if (!is_empty(rootdir->content[i].ptr))
          return 1;
        destroy_file(rootdir->content + i);
        return 0;
      }
    }
    return -ENOENT;
  }
  return 0;
}

static struct inode* srsfs_get_inode(
    struct mnt_idmap* idmap,
    struct super_block* sb,
    const struct inode* dir,
    umode_t mode,
    int i_ino
) {
  struct inode* inode = new_inode(sb);
  if (inode == NULL)
    LOG("failed to create inode\n");
  else {
    inode_init_owner(idmap, inode, dir, mode);
    inode->i_ino = i_ino;
    inode->i_op = &srsfs_inode_ops;
    inode->i_fop = &srsfs_dir_ops;
  }
  return inode;
}

static int srsfs_fill_super(struct super_block* sb, void* data, int silent) {
  struct mnt_idmap* idmap = &nop_mnt_idmap;
  struct inode* inode = srsfs_get_inode(idmap, sb, NULL, S_IFDIR, SRSFS_ROOT_ID);

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

static void prepare_lists(void) {
  for (int i = 0; i < 100; ++i) {
    dirs[i].state = UNUSED;
    for (int j = 0; j < SRSFS_DIR_CAP; ++j) {
      destroy_file(dirs[i].content + j);
    }
  }
}

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
  prepare_lists();
  rootdir = dirs;
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
