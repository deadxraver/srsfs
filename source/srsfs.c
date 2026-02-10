#include "srsfs.h"

#include "flist.h"
#include "srsfs_dbg_logs.h"
#include "srsfs_futil.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

static struct srsfs_file rootdir;
static int fcnt = 0;

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
  if (ii->is_dir) {
    flist_init(&ii->dir_content);
    inode->i_fop = &srsfs_dir_ops;
  } else {
    inode->i_fop = &srsfs_file_ops;
    ii->data = NULL;
  }
  inode->i_op = &srsfs_inode_ops;
  inode->i_private = ii;
  return inode;
}

inline struct inode* srsfs_get_inode(struct super_block* sb, struct srsfs_file* file) {
  return ilookup(sb, file->i_ino);
}

static int srsfs_link(
    struct dentry* old_dentry, struct inode* parent_dir, struct dentry* new_dentry
) {
  // const char* name = new_dentry->d_name.name;
  // struct inode* old_inode = d_inode(old_dentry);
  // struct srsfs_file* old_file = NULL;  // getfile(old_inode->i_ino);
  // if (S_ISDIR(old_inode->i_mode))
  //   return -EPERM;
  // ino_t root = parent_dir->i_ino;
  // struct srsfs_file* rdir = root == SRSFS_ROOT_ID ? rootdir : getfile(root)->ptr;
  // for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
  //   if (rdir->content[i].state == UNUSED)
  //     continue;
  //   if (strcmp(name, rdir->content[i].name) == 0)
  //     return -EEXIST;
  // }
  // for (size_t i = 0; i < SRSFS_DIR_CAP; ++i) {
  //   if (rdir->content[i].state == USED)
  //     continue;
  //   init_file(rdir->content + i, name, 0);
  //   strncpy(rdir->content[i].name, name, SRSFS_FILENAME_LEN);
  //   rdir->content[i].name[SRSFS_FILENAME_LEN - 1] = 0;
  //   rdir->content[i].sd = old_file->data;
  //   rdir->content[i].id = old_file->id;
  //   ++(old_file->data->refcount);
  //   struct inode* new_inode = srsfs_get_inode(
  //       &nop_mnt_idmap, parent_dir->i_sb, parent_dir, old_inode->i_mode, old_inode->i_ino
  //   );
  //   set_nlink(new_inode, rdir->content[i].sd->refcount);
  //   set_nlink(old_inode, rdir->content[i].sd->refcount);
  //   d_instantiate(new_dentry, new_inode);
  //   return 0;
  // }
  return -ENOSPC;
}

static ssize_t srsfs_read(struct file* filp, char* buffer, size_t len, loff_t* offset) {
  return -1;
  // struct inode* inode = filp->f_inode;
  // struct srsfs_file* f = NULL;  // getfile(inode->i_ino);
  // if (f == NULL)
  //   return -ENOENT;
  // if (f->is_dir)
  //   return -EISDIR;
  // if (f->data == NULL)
  //   return -EIO;
  // struct shared_data* sd = f->data;
  // loff_t local_offset = *offset;
  // while (sd != NULL && local_offset >= sd->sz) {
  //   local_offset -= sd->sz;
  //   sd = sd->next;
  // }
  // if (sd == NULL)
  //   return 0;
  // size_t to_read = min(len, sd->sz - local_offset);
  // if (copy_to_user(buffer, sd->data + local_offset, to_read))
  //   return -EFAULT;
  // size_t total_read = to_read;
  // while (total_read < len && sd->next != NULL) {
  //   sd = sd->next;
  //   size_t chunk = min(len - total_read, sd->sz);
  //   if (chunk == 0)
  //     break;
  //   if (copy_to_user(buffer + total_read, sd->data, chunk))
  //     return -EFAULT;
  //   total_read += chunk;
  // }

  // *offset += total_read;
  // return total_read;
}

static ssize_t srsfs_write(struct file* filp, const char* buffer, size_t len, loff_t* offset) {
  return -1;
  // struct inode* inode = filp->f_inode;
  // struct srsfs_file* f = NULL;  // getfile(inode->i_ino);
  // if (f == NULL)
  //   return -ENOENT;
  // if (f->is_dir)
  //   return -EISDIR;

  // if (f->data == NULL) {
  //   f->data = kvmalloc(sizeof(struct shared_data), GFP_KERNEL);
  //   if (!f->data)
  //     return -ENOMEM;
  //   f->data->refcount = 1;
  //   f->data->sz = 0;
  //   f->data->next = NULL;
  // }
  // struct shared_data* sd = f->data;
  // loff_t local_offset = *offset;
  // while (local_offset >= SRSFS_FSIZE) {
  //   local_offset -= SRSFS_FSIZE;
  //   if (sd->next == NULL) {
  //     sd->next = kvmalloc(sizeof(struct shared_data), GFP_KERNEL);
  //     if (!sd->next)
  //       return -ENOMEM;
  //     sd->next->refcount = 1;
  //     sd->next->sz = 0;
  //     sd->next->next = NULL;
  //   }
  //   sd = sd->next;
  // }
  // size_t total_written = 0;
  // while (total_written < len) {
  //   size_t to_write = len - total_written < SRSFS_FSIZE - local_offset ? len - total_written
  //                                                                      : SRSFS_FSIZE -
  //                                                                      local_offset;

  //   if (copy_from_user(sd->data + local_offset, buffer + total_written, to_write))
  //     return -EFAULT;
  //   if (local_offset + to_write > sd->sz)
  //     sd->sz = local_offset + to_write;
  //   total_written += to_write;
  //   local_offset = 0;
  //   if (total_written < len) {
  //     if (sd->next == NULL) {
  //       sd->next = kvmalloc(sizeof(struct shared_data), GFP_KERNEL);
  //       if (sd->next == NULL)
  //         return -ENOMEM;
  //       sd->next->refcount = 1;
  //       sd->next->sz = 0;
  //       sd->next->next = NULL;
  //     }
  //     sd = sd->next;
  //   }
  // }

  // *offset += total_written;
  // return total_written;
}

static int srsfs_iterate(struct file* filp, struct dir_context* ctx) {
  struct dentry* dentry = filp->f_path.dentry;
  struct inode* inode = d_inode(dentry);
  struct inode* parent_inode = d_inode(dentry->d_parent);
  ino_t ino = inode->i_ino;
  int cnt = 0;

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

  LOG("srsfs_iterate: struct srsfs_inode* i = 0x%lx", parent_inode);
  LOG("srsfs_iterate: struct srsfs_inode_info* ii = 0x%lx", parent_inode->i_private);
  if (parent_inode == NULL)
    return 0;
  struct flist* list = &((struct srsfs_inode_info*)parent_inode->i_private)->dir_content;
  LOG("ls");
  print_list(list);
  for (size_t i = ctx->pos - 2;; ++i) {
    struct srsfs_file* f = flist_get(list, i);
    if (f == NULL)
      break;
    if (!dir_emit(ctx, f->name, strlen(f->name), f->i_ino, f->is_dir ? DT_DIR : DT_REG))
      return 0;
    ctx->pos++;
    ++cnt;
  }

  return cnt;
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
  if (inode == NULL)
    kvfree(inode);
  if (f) {
    destroy_file(f);
    kvfree(f);
  }
  return -ENOMEM;
}

static int srsfs_unlink(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  // TODO: same
  // for (size_t i = 0; i < 100; ++i) {
  //   if (dirs[i].state == UNUSED || dirs[i].id != root)
  //     continue;
  //   for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
  //     if (dirs[i].content[j].state == UNUSED)
  //       continue;
  //     if (strcmp(dirs[i].content[j].name, name))
  //       continue;
  //     destroy_file(dirs[i].content + j);
  //     return 0;
  //   }
  // }
  return -ENOENT;
}

static int srsfs_mkdir(
    struct mnt_idmap* idmap, struct inode* parent_inode, struct dentry* child_dentry, umode_t mode
) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  // TODO:
  // for (size_t i = 0; i < 100; ++i) {
  //   if (dirs[i].state == UNUSED || dirs[i].id != root)
  //     continue;
  //   for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
  //     if (dirs[i].content[j].state == USED)
  //       continue;
  //     struct srsfs_dir* dir = alloc_dir();
  //     if (dir == NULL)
  //       return -ENOMEM;
  //     init_dir(dir, name, dirs[i].content + j);
  //     struct inode* inode = srsfs_get_inode(
  //         idmap, parent_inode->i_sb, NULL, S_IFDIR | S_IRWXUGO, dirs[i].content[j].id
  //     );
  //     d_add(child_dentry, inode);
  //     return 0;
  //   }
  // }
  return -ENOENT;
}

static int srsfs_rmdir(struct inode* parent_inode, struct dentry* child_dentry) {
  const char* name = child_dentry->d_name.name;
  ino_t root = parent_inode->i_ino;
  // TODO:
  // for (size_t i = 0; i < 100; ++i) {
  //   if (dirs[i].state == UNUSED || dirs[i].id != root)
  //     continue;
  //   for (size_t j = 0; j < SRSFS_DIR_CAP; ++j) {
  //     if (dirs[i].content[j].state == UNUSED || !dirs[i].content[j].is_dir ||
  //         strcmp(dirs[i].content[j].name, name))
  //       continue;
  //     if (!is_empty(dirs[i].content[j].ptr))
  //       return -1;
  //     destroy_file(dirs[i].content + j);
  //     return 0;
  //   }
  // }
  return -ENOENT;
}

static int srsfs_fill_super(struct super_block* sb, void* data, int silent) {
  init_dir(&rootdir, "srsfs", ALLOC_ID());
  struct inode* inode = srsfs_new_inode(sb, NULL, &rootdir);

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
  // TODO: cleanup
  LOG("killed sb\n");
}

static void test(void) {
  // TEST: correct alloc/free and access
  struct flist list;
  init_file(&rootdir, "srsfs", ALLOC_ID());
  flist_init(&list);
  struct srsfs_file* f = (struct srsfs_file*)kvmalloc(sizeof(*f), GFP_KERNEL);
  init_file(f, "file1", ALLOC_ID());
  flist_push(&list, f);
  f = kvmalloc(sizeof(*f), GFP_KERNEL);
  init_file(f, "file2", ALLOC_ID());
  flist_push(&list, f);
  print_list(&list);
  while (f = flist_pop(&list)) {
    destroy_file(f);
    kvfree(f);
  }
  fcnt = SRSFS_ROOT_ID;  // set back
}

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
  register_filesystem(&srsfs_fs_type);
  LOG("SRSFS successfully registered\n");
  return 0;
}

static void __exit srsfs_exit(void) {
  kvfree(rootdir.name);
  rootdir.name = NULL;
  // TODO: cleanup
  unregister_filesystem(&srsfs_fs_type);
  LOG("SRSFS unregistered successfully\n");
  // destroy_dir(&rootdir);
  LOG("SRSFS left the kernel\n");
}

module_init(srsfs_init);
module_exit(srsfs_exit);
