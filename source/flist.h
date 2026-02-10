#ifndef _FLIST_H

#define _FLIST_H

#include "srsfs_ds.h"

void flist_init(struct flist* head);

bool flist_push(struct flist* head, struct srsfs_file* file);

bool flist_remove(struct flist* head, struct srsfs_file* file);

struct srsfs_file* flist_get(struct flist* head, size_t index);

struct srsfs_file* flist_pop(struct flist* head);

struct flist* flist_iterate(struct flist* head, struct flist* cur);

#endif  // !_FLIST_H
