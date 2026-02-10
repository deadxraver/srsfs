#include "list.h"

#include <linux/mm.h>

#define LOG(fmt, ...) pr_info("[srsfs/flist]: " fmt, ##__VA_ARGS__)

/**
 * Initializes given memory as an empty list.
 */
void flist_init(struct flist* head) {
  if (head == NULL)
    LOG("flist_init: head is NULL!!!");
  head->content = NULL;
  head->next = head;
  head->prev = NULL;
}

/**
 * Tries to push given file to list.
 * On success returns true, on fault - false.
 * NOTE: NULL is not checked for easier debug.
 */
bool flist_push(struct flist* head, struct srsfs_file* file) {
  if (head == NULL)
    LOG("flist_push: head is NULL!!!");
  struct flist* new_node = (struct flist*)kvmalloc(sizeof(struct flist), GFP_KERNEL);
  if (new_node == NULL) {
    LOG("flist_push: could not allocate memory for new node");
    return false;
  }
  new_node->content = file;
  new_node->next = head->next;
  new_node->prev = head;
  head->next = new_node;
  new_node->next->prev = new_node;
  return true;
}

/**
 * Try to remove file from list.
 * If no such element, returns false.
 * NOTE: NULL not checked.
 */
bool flist_remove(struct flist* head, struct srsfs_file* file) {
  if (head == NULL)
    LOG("flist_remove: head is NULL!!!");
  for (struct flist* node = head->next; node != head; node = node->next) {
    if (node->content == file) {
      node->next->prev = node->prev;
      node->prev->next = node->next;
      kvfree(node);
      return true;
    }
  }
  return false;  // not found
}

/**
 * Get file pointer at index.
 * If index >= list size, return NULL.
 */
struct srsfs_file* flist_get(struct flist* head, size_t index) {
  if (head == NULL)
    LOG("flist_get: head is NULL!!!");
  size_t i = 0;
  for (struct flist* node = head->next; node != head; node = node->next, ++i) {
    if (i == index)
      return node->content;
  }
  return NULL;
}

/**
 * Removes first element and returns it as a result.
 * NULL on empty list.
 */
struct srsfs_file* flist_pop(struct flist* head) {
  if (head->next == head)
    return NULL;
  struct flist* elem = head->next;
  struct srsfs_file* res = elem->content;
  head->next = elem->next;
  elem->next->prev = head;
  kvfree(elem);
  elem = NULL;
  return res;
}

/**
 * Iterator for less costly list traversion.
 * Put current node to current to get next.
 * Returns NULL when next element is head.
 */
struct flist* flist_iterate(struct flist* head, struct flist* cur) {
  if (cur->next == head)
    return NULL;
  return cur->next;
}

/**
 * Check whether the given list is empty.
 */
bool flist_is_empty(struct flist* head) {
  return head->next == head;
}

void sd_init(struct shared_data* sd) {
  sd->refcount = 1;
  sd->data = NULL;
  sd->sz = 0;
  sd->capacity = 0;
}
