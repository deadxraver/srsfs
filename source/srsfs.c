#include "srsfs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

static struct file_system_type srsfs_fs_type;

static struct srsfs_dir* rootdir;
static struct srsfs_dir dirs[100];
static int fcnt = 0;

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

static void free_shared(struct shared_data* node, bool force) {
  if (node == NULL || (--(node->refcount) > 0 && !force))
    return;
  free_shared(node->next, 1);
  vfree(node);
}

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
) {
  const char* name = new_dentry->d_name.name;
  struct inode* old_inode = d_inode(old_dentry);
  struct srsfs_file* old_file = getfile(old_inode->i_ino);
  if (S_ISDIR(old_inode->i_mode))
    return -EPERM;
  ino_t root = parent_dir->i_ino;
  struct srsfs_dir* rdir = root == SRSFS_ROOT_ID ? rootdir : getfile(root)->ptr;
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    if (rdir->content[i].state == UNUSED)
      continue;
    if (strcmp(name, rdir->content[i].name) == 0)
      return -EEXIST;
  }
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    if (rdir->content[i].state == USED)
      continue;
    init_file(rdir->content + i, name, 0);
    strncpy(rdir->content[i].name, name, SRSFS_FILENAME_LEN);
    rdir->content[i].name[SRSFS_FILENAME_LEN - 1] = 0;
    rdir->content[i].sd = old_file->sd;
    rdir->content[i].id = old_file->id;
    ++(old_file->sd->refcount);
    struct inode* new_inode = srsfs_get_inode(
        &nop_mnt_idmap, parent_dir->i_sb, parent_dir, old_inode->i_mode, old_inode->i_ino
    );
    set_nlink(new_inode, rdir->content[i].sd->refcount);
    set_nlink(old_inode, rdir->content[i].sd->refcount);
    d_instantiate(new_dentry, new_inode);
    return 0;
  }
  return -ENOSPC;
}

struct srsfs_file* getfile(int ino) {
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == UNUSED)
        continue;
      if (dirs[i].content[j].id == ino)
        return dirs[i].content + j;
    }
  }
  return NULL;
}

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset) {
  struct inode* inode = filp->f_inode;
  struct srsfs_file* f = getfile(inode->i_ino);
  if (f == NULL)
    return -ENOENT;
  if (f->is_dir)
    return -EISDIR;
  if (f->sd == NULL)
    return -EIO;
  struct shared_data* sd = f->sd;
  loff_t local_offset = *offset;
  while (sd != NULL && local_offset >= sd->sz) {
    local_offset -= sd->sz;
    sd = sd->next;
  }
  if (sd == NULL)
    return 0;
  size_t to_read = min(len, sd->sz - local_offset);
  if (copy_to_user(buffer, sd->data + local_offset, to_read))
    return -EFAULT;
  size_t total_read = to_read;
  while (total_read < len && sd->next != NULL) {
    sd = sd->next;
    size_t chunk = min(len - total_read, sd->sz);
    if (chunk == 0)
      break;
    if (copy_to_user(buffer + total_read, sd->data, chunk))
      return -EFAULT;
    total_read += chunk;
  }

  *offset += total_read;
  return total_read;
}

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset) {
  struct inode* inode = filp->f_inode;
  struct srsfs_file* f = getfile(inode->i_ino);
  if (f == NULL)
    return -ENOENT;
  if (f->is_dir)
    return -EISDIR;

  if (f->sd == NULL) {
    f->sd = vmalloc(sizeof(struct shared_data));
    if (!f->sd)
      return -ENOMEM;
    f->sd->refcount = 1;
    f->sd->sz = 0;
    f->sd->next = NULL;
  }
  struct shared_data* sd = f->sd;
  loff_t local_offset = *offset;
  while (local_offset >= SRSFS_FSIZE) {
    local_offset -= SRSFS_FSIZE;
    if (sd->next == NULL) {
      sd->next = vmalloc(sizeof(struct shared_data));
      if (!sd->next)
        return -ENOMEM;
      sd->next->refcount = 1;
      sd->next->sz = 0;
      sd->next->next = NULL;
    }
    sd = sd->next;
  }
  size_t total_written = 0;
  while (total_written < len) {
    size_t to_write = len - total_written < SRSFS_FSIZE - local_offset ? len - total_written
                                                                       : SRSFS_FSIZE - local_offset;

    if (copy_from_user(sd->data + local_offset, buffer + total_written, to_write))
      return -EFAULT;
    if (local_offset + to_write > sd->sz)
      sd->sz = local_offset + to_write;
    total_written += to_write;
    local_offset = 0;
    if (total_written < len) {
      if (sd->next == NULL) {
        sd->next = vmalloc(sizeof(struct shared_data));
        if (sd->next == NULL)
          return -ENOMEM;
        sd->next->refcount = 1;
        sd->next->sz = 0;
        sd->next->next = NULL;
      }
      sd = sd->next;
    }
  }

  *offset += total_written;
  return total_written;
}

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

  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED || dirs[i].id != ino)
      continue;
    for (size_t j = ctx->pos - 2; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == UNUSED)
        continue;
      if (!dir_emit(
              ctx,
              dirs[i].content[j].name,
              strlen(dirs[i].content[j].name),
              dirs[i].content[j].id,
              (dirs[i].content[j].is_dir ? DT_DIR : DT_REG)
          ))
        return 0;
      ctx->pos++;
    }
  }

  return -ENOENT;
}

static void init_file(struct srsfs_file* file, const char* name, bool do_alloc) {
  if (file->state == USED)
    return;
  file->state = USED;
  strncpy(file->name, name, SRSFS_FILENAME_LEN);
  file->name[SRSFS_FILENAME_LEN - 1] = 0;
  file->is_dir = 0;
  file->id = SRSFS_ROOT_ID + ++fcnt;
  if (do_alloc) {
    file->sd = (struct shared_data*)vmalloc(sizeof(struct shared_data));
    file->sd->refcount = 1;
    file->sd->sz = 0;
    file->sd->next = NULL;
  } else
    file->sd = NULL;
}

static void destroy_file(struct srsfs_file* file) {
  if (file->state == UNUSED)
    return;
  file->state = UNUSED;
  memset(file->name, 0, SRSFS_FILENAME_LEN);
  if (file->is_dir)
    destroy_dir(file->ptr);
  file->is_dir = 0;
  file->id = 0;
  if (file->sd == NULL)
    return;
  free_shared(file->sd, 0);
  file->sd = NULL;
}

static void init_dir(struct srsfs_dir* dir, const char* name, struct srsfs_file* assoc_file) {
  if (assoc_file) {
    init_file(assoc_file, name, false);
    assoc_file->is_dir = 1;
    assoc_file->ptr = dir;
    dir->id = assoc_file->id;
  } else
    dir->id = SRSFS_ROOT_ID + ++fcnt;
  dir->state = USED;
  for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
    dir->content[i].state = UNUSED;
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
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED || dirs[i].id != root)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == UNUSED)
        continue;
      if (strcmp(dirs[i].content[j].name, name))
        continue;
      struct inode* inode = srsfs_get_inode(
          &nop_mnt_idmap,
          parent_inode->i_sb,
          NULL,
          dirs[i].content[j].is_dir ? S_IFDIR : S_IFREG,
          dirs[i].content[j].id
      );
      d_add(child_dentry, inode);
      return NULL;
    }
  }
  return NULL;
}

static int srsfs_create(
    struct mnt_idmap* idmap,
    struct inode* parent_inode,
    struct dentry* child_dentry,
    umode_t mode,
    bool b
) {
  ino_t root = parent_inode->i_ino;
  const char* name = child_dentry->d_name.name;
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED)
      continue;
    if (dirs[i].id == root) {
      for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
        if (dirs[i].content[j].state == USED)
          continue;
        init_file(dirs[i].content + j, name, 1);
        struct inode* inode = srsfs_get_inode(
            idmap, parent_inode->i_sb, NULL, S_IFREG | S_IRWXUGO, dirs[i].content[j].id
        );
        d_add(child_dentry, inode);
        return 0;
      }
    }
  }
  return -ENOMEM;
}

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED || dirs[i].id != root)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == UNUSED)
        continue;
      if (strcmp(dirs[i].content[j].name, name))
        continue;
      destroy_file(dirs[i].content + j);
      return 0;
    }
  }
  return -ENOENT;
}

static int srsfs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED || dirs[i].id != root)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == USED)
        continue;
      struct srsfs_dir* dir = alloc_dir();
      if (dir == NULL)
        return -ENOMEM;
      init_dir(dir, name, dirs[i].content + j);
      struct inode* inode = srsfs_get_inode(
          idmap, parent_inode->i_sb, NULL, S_IFDIR | S_IRWXUGO, dirs[i].content[j].id
      );
      d_add(child_dentry, inode);
      return 0;
    }
  }
  return -ENOENT;
}

static int srsfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED || dirs[i].id != root)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      if (dirs[i].content[j].state == UNUSED || !dirs[i].content[j].is_dir ||
          strcmp(dirs[i].content[j].name, name))
        continue;
      if (!is_empty(dirs[i].content[j].ptr))
        return -1;
      destroy_file(dirs[i].content + j);
      return 0;
    }
  }
  return -ENOENT;
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
    if (S_ISDIR(mode))
      inode->i_fop = &srsfs_dir_ops;
    else
      inode->i_fop = &srsfs_file_ops;
    inode->i_ino = i_ino;
    inode->i_op = &srsfs_inode_ops;
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
      dirs[i].content[j].is_dir = 0;  // prevent from dereferencing garbage pointers
      dirs[i].content[j].sd = NULL;   // prevent free on garbage pointer
      destroy_file(dirs[i].content + j);
    }
  }
}

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
  prepare_lists();
  rootdir = dirs;
  rootdir->state = USED;
  rootdir->id = SRSFS_ROOT_ID;
  set_fs_params();
  register_filesystem(&srsfs_fs_type);
  LOG("SRSFS successfully registered\n");
  return 0;
}

static void __exit srsfs_exit(void) {
  unregister_filesystem(&srsfs_fs_type);
  LOG("SRSFS unregistered successfully\n");
  for (size_t i = 0; i < 100; ++i) {
    if (dirs[i].state == UNUSED)
      continue;
    for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
      destroy_file(dirs[i].content + j);
    }
  }
  LOG("SRSFS left the kernel\n");
}

module_init(srsfs_init);
module_exit(srsfs_exit);
