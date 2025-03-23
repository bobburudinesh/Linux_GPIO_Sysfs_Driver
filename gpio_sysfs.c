#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/platform_device.h>
#include<linux/string.h>
#include<linux/fs.h>
#include<asm/uaccess.h>
#include<linux/init.h>
#include<linux/device.h>
#include<linux/io.h>
#include<linux/of.h>
#include<linux/of_device.h>


#undef pr_fmt
#define pr_fmt(fmt) "%s : "fmt,__func__



int gpio_sysfs_probe(struct platform_device *pdev);
int gpio_sysfs_remove(struct platform_device *pdev);



/*GPIO Device private data structure*/
struct gpiodev_private_data
{
	char label[20];
};

/*Driver private data structure*/
struct gpiodrv_private_data
{
	int total_devices;
	struct class *class_gpio;
};

struct gpiodrv_private_data	gpio_drv_data;



int gpio_sysfs_probe(struct platform_device *pdev) {

	return 0;
}

int gpio_sysfs_remove(struct platform_device *pdev) {
	
	return 0;
}


struct of_device_id gpio_device_match[] = {
	{.compatible = "bone-gpio-sysfs"},
	{ }
};


struct platform_driver gpio_sysfs_platform_driver = {
	.probe = gpio_sysfs_probe,
	.remove = gpio_sysfs_remove,
	.driver = {
		.name = "bone-gpio-sysfs",
		.of_match_table = of_match_ptr(gpio_device_match)
	}

};

int __init gpio_sysfs_init(void) {
	
	gpio_drv_data.class_gpio = class_create(THIS_MODULE, "bone_gpios");
       if(IS_ERR(gpio_drv_data.class_gpio)) {
       	pr_info("Error in creating class\n");
	return PTR_ERR(gpio_drv_data.class_gpio);
       }	
	platform_driver_register(&gpio_sysfs_platform_driver);
	pr_info("module load success");
	return 0;
}

void __exit gpio_sysfs_exit(void) {
	platform_driver_unregister(&gpio_sysfs_platform_driver);
	class_destroy(gpio_drv_data.class_gpio);
}

module_init(gpio_sysfs_init);
module_exit(gpio_sysfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dinesh Bobburu");
MODULE_DESCRIPTION("A gpio sysfs driver");
