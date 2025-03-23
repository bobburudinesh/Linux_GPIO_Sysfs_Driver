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
#include<linux/gpio/consumer.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s : "fmt,__func__



int gpio_sysfs_probe(struct platform_device *pdev);
int gpio_sysfs_remove(struct platform_device *pdev);
ssize_t gpio_direction_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t gpio_value_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t gpio_label_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t gpio_direction_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t gpio_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);



/*GPIO Device private data structure*/
struct gpiodev_private_data
{
	char label[20];
	struct gpio_desc *desc;
};

/*Driver private data structure*/
struct gpiodrv_private_data
{
	int total_devices;
	struct class *class_gpio;
	struct device **dev;
};

struct gpiodrv_private_data	gpio_drv_data;


ssize_t gpio_direction_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
	int dir;
	char* direction;
	dir = gpiod_get_direction(dev_data->desc);
	if(dir < 0)
		return dir;
	/*if dir = 0, then show "out", if dir = 1, then show "in"*/
	direction = (dir == 0) ? "out" : "in";
	return sprintf(buf,"%s\n", direction);
}
ssize_t gpio_value_show(struct device *dev, struct device_attribute *attr, char *buf) {
	int value;
	struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
	value = gpiod_get_value(dev_data->desc);
	return sprintf(buf,"%d\n", value);
}

ssize_t gpio_label_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", dev_data->label);
}


ssize_t gpio_direction_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	int ret;

	struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
	if(sysfs_streq(buf, "in"))
		ret = gpiod_direction_input(dev_data->desc);
	else if (sysfs_streq(buf,"out"))
		ret = gpiod_direction_output(dev_data->desc, 0);
	else
		ret = -EINVAL;
        return ret ? ret : count;
}


ssize_t gpio_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	int ret;
        long value;
        struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
        ret = kstrtol(buf, 0, &value);
        if(ret)
                return ret;
        gpiod_set_value(dev_data->desc, value);
        return count;
}

static DEVICE_ATTR_RW(gpio_direction);
static DEVICE_ATTR_RW(gpio_value);
static DEVICE_ATTR_RO(gpio_label);

static struct attribute *gpio_attrs[] = {
	&dev_attr_gpio_direction.attr,
	&dev_attr_gpio_value.attr,
	&dev_attr_gpio_label.attr,
	NULL
};

static struct attribute_group gpio_attr_group = {
	.attrs = gpio_attrs
};

static const struct attribute_group *gpio_attr_groups[] = {
	&gpio_attr_group,
	NULL
};

int gpio_sysfs_probe(struct platform_device *pdev) {
	/*Parent device node*/
	struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;

	struct device *dev = &pdev->dev;
	
	struct gpiodev_private_data *dev_data;	
	
	const char *name;
	
	int i = 0;
	
	int ret;

	gpio_drv_data.total_devices = of_get_child_count(parent);
	if(!gpio_drv_data.total_devices) {
		dev_err(dev,"No devices found\n");
		return -EINVAL;
	} 

	dev_info(dev, "Total devices found = %d\n", gpio_drv_data.total_devices);

	gpio_drv_data.dev = devm_kzalloc(dev, sizeof(struct device*) * gpio_drv_data.total_devices, GFP_KERNEL);

	for_each_available_child_of_node(parent, child){
		dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
		if(!dev_data) {
			dev_err(dev,"cannot allocate memory\n");
			return -ENOMEM;
		}

		if(of_property_read_string(child, "label", &name)){
                	dev_warn(dev,"Missing label information\n");
                	snprintf(dev_data->label, sizeof(dev_data->label), "unkngpio %d", i);
         	} else {	
                	strcpy(dev_data->label, name);
                	dev_info(dev, "GPIO label = %s\n", dev_data->label);
		}
		dev_data->desc = devm_fwnode_get_gpiod_from_child(dev, "bone", &child->fwnode, GPIOD_ASIS,dev_data->label);
		if(IS_ERR(dev_data->desc)) {
			ret = PTR_ERR(dev_data->desc);
			if(ret == -ENOENT){
				dev_err(dev, "No GPIO has been assigned to requested function and/or index\n");
			}
			return ret;
		}

		/*Set GPIO direction to output*/
		gpiod_direction_output(dev_data->desc, 0);
		if(ret) {
			dev_err(dev,"gpio direction set failed\n");
			return ret;
		}
		/*Create devices under /sys/class/bone_gpio */
		gpio_drv_data.dev[i] = device_create_with_groups(gpio_drv_data.class_gpio, dev, 0, dev_data, gpio_attr_groups, dev_data->label);
		if(IS_ERR(gpio_drv_data.dev[i])){
			dev_err(dev, "Error in device_create\n");
			return PTR_ERR(gpio_drv_data.dev[i]);
		}
		
		i++;
	}
	return 0;
}

int gpio_sysfs_remove(struct platform_device *pdev) {
	int i;

	for(i = 0; i<gpio_drv_data.total_devices; i++){
		device_unregister(gpio_drv_data.dev[i]);
	}
	dev_info(&pdev->dev,"Remove Sysfs driver called\n");
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
