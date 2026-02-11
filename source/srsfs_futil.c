#include "srsfs_futil.h"

#include <linux/mm.h>
#include <linux/stddef.h>

#include "list.h"

#define LOG(fmt, ...) pr_info("[srsfs_futil]: " fmt, ##__VA_ARGS__)

void init_file(struct srsfs_file* file, const char* name, int id) {
  file->name = (char*)kvmalloc(strlen(name) + 1, GFP_KERNEL);
  strcpy(file->name, name);
  file->i_ino = id;
}

void init_dir(struct srsfs_file* file, const char* name, int id) {
  init_file(file, name, id);
  file->is_dir = 1;
}

void destroy_file(struct srsfs_file* file) {
  if (file == NULL)
    return;
  kvfree(file->name);
  file->name = NULL;
  file->i_ino = 0;
}

void free_shared(struct shared_data* node, bool force) {
  // if (node == NULL || (--(node->refcount) > 0 && !force))
  //   return;
  // free_shared(node->next, 1);
  // kvfree(node);
}
