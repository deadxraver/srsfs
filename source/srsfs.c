#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "srsfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("deadxraver");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int __init srsfs_init(void) {
  LOG("SRSFS joined the kernel\n");
  return 0;
}

static void __exit srsfs_exit(void) {
  LOG("SRSFS left the kernel\n");
}

module_init(srsfs_init);
module_exit(srsfs_exit);
