#include <linux/module.h>

static int __init helloworld_init(void) {
    pr_info("Hello world from my modules\n");
    return 0;
}

static void __exit helloworld_exit(void) { pr_info("Goodbye my modules\n"); }

module_init(helloworld_init);
module_exit(helloworld_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sang Tan Truong");
MODULE_DESCRIPTION("A simple hello world kernel module");
MODULE_INFO(board, "Beaglebone black Rev C");
