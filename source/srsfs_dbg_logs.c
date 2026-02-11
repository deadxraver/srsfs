#include "srsfs_dbg_logs.h"

#define LOG(fmt, ...) pr_info("[srsfs/logger]: " fmt, ##__VA_ARGS__)

void print_list(struct flist* head) {
  if (head == NULL) {
    LOG("head is NULL!!!");
    return;
  }
  LOG("0x%lx {", head);
  for (struct flist* node = head->next; node != head; node = node->next) {
    LOG("   file {name='%s' ino=%d},", node->content->name, node->content->i_ino);
  }
  LOG("}");
}
