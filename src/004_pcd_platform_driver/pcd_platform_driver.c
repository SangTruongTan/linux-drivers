#include <linux/module.h>
#include <linux/platform_device.h>

// 1. Create 2 platform devices

struct platform_device platform_pcdev_1 = {.name = "pseudo-char-device",
                                           .id = 0};

struct platform_device platform_pcdev_2 = {.name = "pseudo-char-device",
                                           .id = 1};

static int __int pcdev_platform_init(void) {
    // Register platform devices
    platform_device_register(&platform_pcdev_1);
    platform_device_register(&platform_pcdev_2);
    return 0;
}

static void __exit pcdev_paltform_exit(void: {
    platform_device_unregister(&platform_pcdev_1);
    platform_device_unregister(&platform_pcdev_2);

}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module which registers platform devices");
