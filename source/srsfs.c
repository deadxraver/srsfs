#include "srsfs.h"

#include "list.h"
#include "srsfs_dbg_logs.h"
#include "srsfs_futil.h"

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

static void srsfs_inc_rc(struct inode* inode) {
  if (inode == NULL) {
    LOG("srsfs_inc_rc: inode is NULL");
    return;
  }
  LOG("0x%lx: rc=%d", inode, inode->i_nlink);
  inode_inc_link_count(inode);
  LOG("srsfs_dec_rc: incremented, now %d", inode->i_nlink);
}

static void srsfs_dec_rc(struct inode* inode) {
  if (inode == NULL) {
    LOG("srsfs_dec_rc: inode is NULL");
    return;
  }
  LOG("0x%lx: rc=%d", inode, inode->i_nlink);
  if (inode->i_nlink <= 1) {
    if (inode->i_private == NULL)
      goto done;
    struct srsfs_inode_info* ii = (struct srsfs_inode_info*)inode->i_private;
    if (ii->data.data)
      kvfree(ii->data.data);
    ii->data.data = NULL;
    ii->data.sz = 0;
    ii->data.capacity = 0;
    kvfree(ii);
    inode->i_private = NULL;
    LOG("srsfs_dec_rc: deleted file contents for 0x%lx", inode);
  }
done:
  inode_dec_link_count(inode);
  LOG("srsfs_dec_rc: decremented");
}

static struct inode* srsfs_new_inode(
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
  if (ii->is_dir) {
    flist_init(&ii->dir_content);
    inode->i_fop = &srsfs_dir_ops;
    inode->i_size = PAGE_SIZE;
  } else {
    sd_init(&ii->data);
    inode->i_fop = &srsfs_file_ops;
    inode->i_size = 0;
  }
  inode->i_op = &srsfs_inode_ops;
  inode->i_private = ii;
  return inode;
}

static inline struct inode* srsfs_get_inode(struct super_block* sb, struct srsfs_file* file) {
  return ilookup(sb, file->i_ino);
}

void srsfs_rec_cleanup(struct inode* inode) {
  if (inode == NULL)
    return;
  LOG("srsfs_rec_cleanup: starting cleanup for 0x%lx", inode);
  struct srsfs_inode_info* ii = (struct srsfs_inode_info*)inode->i_private;
  if (ii == NULL) {
    LOG("srsfs_rec_cleanup: ii already NULL");
    return;
  }
  if (ii->is_dir) {
    struct flist* list = &ii->dir_content;
    for (struct srsfs_file* f = flist_pop(list); f != NULL; f = flist_pop(list)) {
      LOG("srsfs_rec_cleanup: deleting file %s", f->name);
      struct inode* finode = srsfs_get_inode(inode->i_sb, f);
      srsfs_rec_cleanup(finode);
      destroy_file(f);
      kvfree(f);
      f = NULL;
    }
    LOG("srsfs_rec_cleanup: inode 0x%lx dir contents freed", inode);
  } else {
    if (ii->data.data)
      kvfree(ii->data.data);
    ii->data.data = NULL;
    ii->data.sz = 0;
    ii->data.capacity = 0;
    LOG("srsfs_rec_cleanup: inode 0x%lx file contents freed", inode);
  }
  kvfree(ii);
  inode->i_private = NULL;
  LOG("srsfs_rec_cleanup: inode 0x%lx cleared", inode);
}

void srsfs_cleanup(void) {
  srsfs_rec_cleanup(root_inode);
  root_inode = NULL;
  destroy_file(&rootdir);
}

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
) {
  const char* name = new_dentry->d_name.name;
  struct inode* old_inode = d_inode(old_dentry);
  if (S_ISDIR(old_inode->i_mode))
    return -EPERM;
  struct flist* list = &((struct srsfs_inode_info*)parent_dir->i_private)->dir_content;
  for (struct flist* node = flist_iterate(list, list); node != NULL;
       node = flist_iterate(list, node)) {
    if (strcmp(node->content->name, name) == 0)
      return -EEXIST;
  }
  if (!igrab(old_inode))
    return -ENOENT;
  struct srsfs_file* f = NULL;
  f = (struct srsfs_file*)kvmalloc(sizeof(*f), GFP_KERNEL);
  if (f == NULL)
    goto mem;
  init_file(f, name, old_inode->i_ino);
  if (!flist_push(list, f))
    goto mem;
  LOG("new link %s", name);
  srsfs_inc_rc(old_inode);
  d_add(new_dentry, old_inode);
  return 0;
mem:
  if (f) {
    destroy_file(f);
    kvfree(f);
  }
  return -ENOMEM;
}

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset) {
  struct inode* inode = filp->f_inode;
  struct srsfs_inode_info* ii = (struct srsfs_inode_info*)inode->i_private;
  if (ii->is_dir)
    return -EISDIR;
  struct shared_data* sd = &ii->data;
  LOG("sd->data addr: 0x%lx", sd->data);
  if (sd->data == NULL)
    return 0;  // empty file
  loff_t local_offset = *offset;
  size_t to_read = min(sd->sz - local_offset, len);
  LOG("srsfs_read: sd->sz=%ld,sd->cap=%ld,len=%ld,off=%ld,to_read=%ld",
      sd->sz,
      sd->capacity,
      len,
      local_offset,
      to_read);
  if (copy_to_user(buffer, sd->data + local_offset, to_read))
    return -EFAULT;
  *offset += to_read;
  return to_read;
}

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset) {
  struct inode* inode = filp->f_inode;
  struct srsfs_inode_info* ii = (struct srsfs_inode_info*)inode->i_private;
  if (ii->is_dir) {
    LOG("srsfs_write: is a directory");
    return -EISDIR;
  }
  struct shared_data* sd = &ii->data;
  loff_t local_offset = *offset;
  LOG("srsfs_write: sd->sz=%ld,sd->cap=%ld,len=%ld,off=%ld", sd->sz, sd->capacity, len, local_offset
  );
  char* newdata = kvmalloc(len + local_offset, GFP_KERNEL);
  if (newdata == NULL) {
    LOG("srsfs_write: could not realloc");
    return -ENOMEM;
  }
  if (sd->data) {
    memcpy(newdata, sd->data, min(sd->sz, len + local_offset));
    kvfree(sd->data);
  }
  sd->data = newdata;
  sd->capacity = len + local_offset;
  LOG("new sd->data addr: 0x%lx", sd->data);
  if (copy_from_user(sd->data + local_offset, buffer, len))
    return -EFAULT;
  sd->sz = sd->capacity;
  inode->i_size = sd->sz;
  *offset += len;

  return len;
}

static int srsfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = d_inode(dentry);
  struct inode* parent_inode = d_inode(dentry->d_parent);
  ino_t ino = inode->i_ino;

  if (ctx->pos == 0) {
    if (!dir_emit(ctx, ".", 1, ino, DT_DIR))
      return 0;
    ctx->pos++;
  }

  if (ctx->pos == 1) {
    ino_t parent_ino = parent_inode->i_ino;
    if (!dir_emit(ctx, "..", 2, parent_ino, DT_DIR))
      return 0;
    ctx->pos++;
  }

  LOG("srsfs_iterate: struct srsfs_inode* i = 0x%lx", filp->f_inode);
  LOG("srsfs_iterate: struct srsfs_inode_info* ii = 0x%lx", filp->f_inode->i_private);
  struct flist* list = &((struct srsfs_inode_info*)filp->f_inode->i_private)->dir_content;
  LOG("ls");
  print_list(list);
  for (size_t i = ctx->pos - 2;; ++i) {
    struct srsfs_file* f = flist_get(list, i);
    if (f == NULL)
      break;
    if (!dir_emit(ctx, f->name, strlen(f->name), f->i_ino, f->is_dir ? DT_DIR : DT_REG))
      return 0;
    ctx->pos++;
  }

  return 0;
}

static struct dentry* srsfs_lookup(
    struct inode* parent_inode, struct dentry* child_dentry, unsigned int flag
) {
  struct srsfs_inode_info* ii = (struct srsfs_inode_info*)parent_inode->i_private;
  struct flist* list = &ii->dir_content;
  const char* name = child_dentry->d_name.name;
  for (struct flist* node = flist_iterate(list, list); node != NULL;
       node = flist_iterate(list, node)) {
    struct srsfs_file* f = node->content;
    if (strcmp(f->name, name))
      continue;
    struct inode* inode = srsfs_get_inode(parent_inode->i_sb, f);
    d_add(child_dentry, inode);
    return NULL;
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
  const char* name = child_dentry->d_name.name;
  struct srsfs_inode_info* ii = (struct srsfs_inode_info*)parent_inode->i_private;
  struct flist* list = &ii->dir_content;
  LOG("starting srsfs_create...");
  print_list(list);
  for (struct flist* node = flist_iterate(list, list); node != NULL;
       node = flist_iterate(list, node)) {
    if (strcmp(node->content->name, name) == 0)
      return -EEXIST;
  }
  struct srsfs_file* f = NULL;
  struct inode* inode = NULL;
  f = (struct srsfs_file*)kvmalloc(sizeof(struct srsfs_file), GFP_KERNEL);
  if (f == NULL)
    goto mem;
  init_file(f, name, ALLOC_ID());
  inode = srsfs_new_inode(NULL, parent_inode, f);
  if (inode == NULL)
    goto mem;
  if (!flist_push(list, f))
    goto mem;
  d_add(child_dentry, inode);
  LOG("Success.");
  print_list(list);
  return 0;
mem:
  if (f) {
    destroy_file(f);
    kvfree(f);
  }
  return -ENOMEM;
}

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  struct srsfs_file* file = NULL;
  struct flist* list = &((struct srsfs_inode_info*)parent_inode->i_private)->dir_content;
  for (struct flist* node = flist_iterate(list, list); node != NULL;
       node = flist_iterate(list, node)) {
    file = node->content;
    if (strcmp(file->name, name))
      continue;
    if (file->is_dir)
      return -EISDIR;
    flist_remove(list, file);
    destroy_file(file);
    kvfree(file);
    file = NULL;
    LOG("unlink: d_inode=0x%lx", d_inode(child_dentry));
    srsfs_dec_rc(d_inode(child_dentry));
    LOG("deleted %s", name);
    return 0;
  }
  return -ENOENT;
}

static int srsfs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  const char* name = child_dentry->d_name.name;
  struct srsfs_file* dir = NULL;
  struct inode* inode = NULL;
  struct flist* parent_dir = &((struct srsfs_inode_info*)parent_inode->i_private)->dir_content;
  for (struct flist* node = flist_iterate(parent_dir, parent_dir); node != NULL;
       node = flist_iterate(parent_dir, node)) {
    struct srsfs_file* f = node->content;
    if (strcmp(f->name, name) == 0)
      return -EEXIST;
  }
  dir = (struct srsfs_file*)kvmalloc(sizeof(*dir), GFP_KERNEL);
  if (dir == NULL)
    goto mem;
  init_dir(dir, name, ALLOC_ID());
  inode = srsfs_new_inode(NULL, parent_inode, dir);
  if (inode == NULL)
    goto mem;
  if (!flist_push(parent_dir, dir))
    goto mem;
  d_add(child_dentry, inode);
  LOG("Successfully created dir %s", dir->name);
  print_list(parent_dir);
  return 0;
mem:
  if (dir != NULL) {
    destroy_file(dir);
    kvfree(dir);
    dir = NULL;
  }
  return -ENOMEM;
}

static int srsfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  struct flist* list = &((struct srsfs_inode_info*)parent_inode->i_private)->dir_content;
  for (struct flist* node = flist_iterate(list, list); node != NULL;
       node = flist_iterate(list, node)) {
    struct srsfs_file* f = node->content;
    if (strcmp(name, f->name))
      continue;
    if (!f->is_dir)
      return -ENOTDIR;
    struct inode* dir_inode = d_inode(child_dentry);
    if (dir_inode == NULL) {
      LOG("Failed to get inode for %d %s", f->i_ino, f->name);
      return -EINVAL;
    }
    if (!flist_is_empty(&((struct srsfs_inode_info*)dir_inode->i_private)->dir_content))
      return -EPERM;
    flist_remove(list, f);
    destroy_file(f);
    kvfree(f);
    return 0;
  }
  return -ENOENT;
}

static int srsfs_fill_super(struct super_block* sb, void* data, int silent) {
  init_dir(&rootdir, "srsfs", ALLOC_ID());
  root_inode = srsfs_new_inode(sb, NULL, &rootdir);

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
  srsfs_cleanup();
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
