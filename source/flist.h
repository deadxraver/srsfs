#ifndef _FLIST_H

#define _FLIST_H

#include "srsfs_ds.h"

/**
 * double-linked round list storing srsfs files.
 * Each node represents single file.
 * Head node should not contain useful info.
 */
struct flist {  // NOTE: should be allocated/freed using kvmalloc/kvfree respectively
  struct srsfs_file* content;
  struct flist* next;
  struct flist* prev;
};

void flist_init(struct flist* head);

bool flist_push(struct flist* head, struct srsfs_file* file);

bool flist_remove(struct flist* head, struct srsfs_file* file);

struct srsfs_file* flist_get(struct flist* head, size_t index);

struct srsfs_file* flist_pop(struct flist* head);

#endif  // !_FLIST_H
