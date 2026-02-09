#include "srsfs_futil.h"

#include <linux/mm.h>
#include <linux/stddef.h>

#include "flist.h"

#define LOG(fmt, ...) pr_info("[srsfs_futil]: " fmt, ##__VA_ARGS__)

void init_file(struct srsfs_file* file, const char* name, int id, bool do_alloc) {
  file->name = (char*)kvmalloc(strlen(name) + 1, GFP_KERNEL);
  strcpy(file->name, name);
  file->is_dir = 0;
  file->id = id;
  if (do_alloc) {
    file->data = (struct shared_data*)kvmalloc(sizeof(struct shared_data), GFP_KERNEL);
    file->data->refcount = 1;
    file->data->sz = 0;
    file->data->next = NULL;
  } else
    file->data = NULL;
}

void destroy_file(struct srsfs_file* file) {
  if (file == NULL)
    return;
  kvfree(file->name);
  file->name = NULL;
  file->id = 0;
  if (file->is_dir)
    destroy_dir(file);
  else {
    file->is_dir = 0;
    if (file->data == NULL)
      return;
    free_shared(file->data, 0);
    file->data = NULL;
  }
}

void init_dir(struct srsfs_file* dir, const char* name, int id) {
  init_file(dir, name, id, false);
  dir->is_dir = 1;
}

void destroy_dir(struct srsfs_file* dir) {  // NOTE: dir itself is not freed
  if (dir == NULL)
    return;
  while (dir->dir_content) {
    struct srsfs_file* head_file = flist_get(dir->dir_content, 0);
    LOG("destroy_dir: struct srsfs_file* head_file = 0x%lx", head_file);
    dir->dir_content = flist_remove(dir->dir_content, head_file);
    destroy_file(head_file);
    kvfree(head_file);
    head_file = NULL;
  }
}

bool is_empty(struct srsfs_file* dir) {
  return dir && dir->is_dir && dir->dir_content == NULL;
}

void free_shared(struct shared_data* node, bool force) {
  if (node == NULL || (--(node->refcount) > 0 && !force))
    return;
  free_shared(node->next, 1);
  kvfree(node);
}
