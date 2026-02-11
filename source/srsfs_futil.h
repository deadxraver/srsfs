#ifndef _SRSFS_FUTIL_H

#define _SRSFS_FUTIL_H

#include "srsfs_ds.h"

void init_file(struct srsfs_file* file, const char* name, int id);

void init_dir(struct srsfs_file* file, const char* name, int id);

void destroy_file(struct srsfs_file* file);

void free_shared(struct shared_data*, bool);

#endif  // !_SRSFS_FUTIL_H
