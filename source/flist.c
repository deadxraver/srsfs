#include "flist.h"

#include <linux/mm.h>

#define SRSFS_ROOT_ID 1000
#define SRSFS_FSIZE 1024  // 1KB

struct flist* flist_push(struct flist* head, struct srsfs_file* file) {
  if (head == NULL) {
    head = (struct flist*)kvmalloc(sizeof(*head), GFP_KERNEL);
    head->next = head;
    head->prev = head;
    head->content = file;
    return head;
  }
  struct flist* new_head = (struct flist*)kvmalloc(sizeof(*head), GFP_KERNEL);
  new_head->next = head;
  new_head->prev = head->prev;
  head->prev = new_head;
  new_head->prev->next = new_head;
  return new_head;
}

struct flist* flist_remove(struct flist* head, struct srsfs_file* file) {
  if (head == NULL)
    return NULL;
  if (head->content == file) {
    head->next->prev = head->prev;
    head->prev->next = head->next;
    struct flist* new_head = head->next;
    // NOTE: file should be (kv)freed outside
    kvfree(head);
    return new_head;
  }
  for (struct flist* node = head->next; node != head; node = node->next) {
    if (node->content == file) {
      node->prev->next = node->next;
      node->next->prev = node->prev;
      head = node->prev;
      // NOTE: free
      kvfree(node);
      return head;
    }
  }
  // not found
  return head;  // or NULL??
}

struct srsfs_file* flist_get(struct flist* head, size_t index) {
  if (head == NULL)
    return NULL;
  if (index == 0)
    return head->content;
  size_t i = 1;  // head is present
  for (struct flist* node = head->next; node != head; node = node->next, ++i) {
    if (i == index)
      return node->content;
  }
  return NULL;
}
