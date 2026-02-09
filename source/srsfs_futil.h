#ifndef _SRSFS_FUTIL_H

#define _SRSFS_FUTIL_H

#include "srsfs_ds.h"

void init_file(struct srsfs_file* file, const char* name, int id, bool do_alloc);

void destroy_file(struct srsfs_file* file);

void init_dir(struct srsfs_file* dir, const char* name, int id);

void destroy_dir(struct srsfs_file* dir);

bool is_empty(struct srsfs_file* dir);

void free_shared(struct shared_data*, bool);

#endif  // !_SRSFS_FUTIL_H
