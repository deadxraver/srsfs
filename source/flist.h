#ifndef _FLIST_H

#define _FLIST_H

#include "srsfs_ds.h"

/**
 * double-linked round list storing srsfs files.
 * Each node represents single file.
 */
struct flist {  // NOTE: should be allocated/freed using kvmalloc/kvfree respectively
  struct srsfs_file* content;
  struct flist* next;
  struct flist* prev;
};

static struct flist* flist_push(struct flist* head, struct srsfs_file* file);

static struct flist* flist_remove(struct flist* head, struct srsfs_file* file);

static struct srsfs_file* flist_get(
    struct flist* head, size_t index
);  // NOTE: list[i] is not guranteed to be the same element between push/remove

#endif  // !_FLIST_H
