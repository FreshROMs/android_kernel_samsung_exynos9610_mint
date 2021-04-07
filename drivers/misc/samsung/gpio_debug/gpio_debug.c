/* Copyright (c) 2014 Samsung Electronics Co., Ltd */

#include <linux/gpio_debug.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>

struct gpio_debug_data;

struct gpio_debug_event {
	int                         gpio;
	int                         gpio_idx;
	struct gpio_debug_event_def def;
	struct gpio_debug_data      *data;
	struct dentry               *file;
};

struct gpio_debug_data {
	int                     gpio_count;
	int                     *gpios;
	struct dentry           *gpio_debug_dir;
	struct dentry           *gpio_debug_events_dir;
	struct platform_device  *pdev;
	struct gpio_debug_event *events;
	int                     event_count;
	int                     event_base;
};

static struct gpio_debug_data debug_data;

DEFINE_MUTEX(debug_lock);

enum {
	GPIO_DEBUG_TOGGLE_100,
	GPIO_DEBUG_TOGGLE_200,
};

static struct gpio_debug_event_def debug_events_table[] = {
	[GPIO_DEBUG_TOGGLE_100] = {
		.name = "toggle100",
		.description = "Toggle the GPIO 100 times at initialisation",
	},
	[GPIO_DEBUG_TOGGLE_200] = {
		.name = "toggle200",
		.description = "Toggle the GPIO 200 times at initialisation",
	},
};

static void gpio_debug_event(int gpio, int state)
{
	if (gpio >= 0)
		gpio_set_value(gpio, state);
}

static void gpio_debug_event_exec(int event_id, int state)
{
	if ((event_id >= 0) && (event_id < debug_data.event_count) && debug_data.events)
		gpio_debug_event(debug_data.events[event_id].gpio, state);
}

void gpio_debug_event_enter(int base, int id)
{
	gpio_debug_event_exec(base + id, 0);
}

void gpio_debug_event_exit(int base, int id)
{
	gpio_debug_event_exec(base + id, 1);
}

int gpio_debug_event_enabled(int base, int id)
{
	int event_id = base + id;

	if ((event_id >= 0) &&
	    (event_id < debug_data.event_count) &&
	    debug_data.events &&
	    debug_data.events[event_id].gpio >= 0)
		return 1;
	else
		return 0;
}

static int gpio_debug_event_link(struct gpio_debug_event *event, int gpio_index)
{
	struct gpio_debug_data *data = event->data;

	if (gpio_index >= data->gpio_count)
		return -ERANGE;

	if (gpio_index >= 0)
		event->gpio = data->gpios[gpio_index];
	else
		event->gpio = -1;

	event->gpio_idx = gpio_index;

	return 0;
}

static ssize_t event_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t                 ret = 0;
	struct gpio_debug_event *event = file->f_inode->i_private;
	char                    buf[256];
	int                     pos;

	mutex_lock(&debug_lock);

	pos = snprintf(buf, sizeof(buf), "Description:\n%s\n\nEvent is mapped to GPIO index %d with number %d\n", event->def.description, event->gpio_idx, event->gpio);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);

	mutex_unlock(&debug_lock);

	return ret;
}

static ssize_t event_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char                    *user_string;
	ssize_t                 ret;
	struct gpio_debug_event *event = file->f_inode->i_private;
	int                     new_index = -1;

	mutex_lock(&debug_lock);

	user_string = kmalloc(count + 1, GFP_KERNEL);
	memory_read_from_buffer(user_string, count, ppos, user_buf, count);

	user_string[count] = '\0';
	ret = (ssize_t)strnlen(user_string, count + 1);

	if (kstrtou32(user_string, 10, &new_index)) {
		return -EINVAL;

	gpio_debug_event_link(event, new_index);

	kfree(user_string);
	mutex_unlock(&debug_lock);

	return ret;
}

static const struct file_operations event_ops = {
	.read = event_read,
	.write = event_write,
};

static void create_event_file(struct gpio_debug_event *event)
{
	struct gpio_debug_data *data = event->data;

	if (data && data->gpio_debug_events_dir) {
		event->file = debugfs_create_file(event->def.name, 0660, data->gpio_debug_events_dir, event, &event_ops);
		if (IS_ERR_OR_NULL(event->file)) {
			event->file = NULL;
			pr_warn("%s: Could not create debugfs file for %s\n", __func__, event->def.name);
		}
	}
}

static void remove_event_file(struct gpio_debug_event *event)
{
	if (event && event->file) {
		debugfs_remove(event->file);
		event->file = NULL;
	}
}

static void gpio_debug_init_event(struct gpio_debug_data *data, struct gpio_debug_event_def *event, struct gpio_debug_event *event_save)
{
	event_save->def.description = event->description;
	event_save->def.name = event->name;
	event_save->gpio = -1;
	event_save->gpio_idx = -1;
	event_save->data = data;

	create_event_file(event_save);
}

static void gpio_debug_destroy_event(struct gpio_debug_event *event)
{
	remove_event_file(event);
	event->def.description = NULL;
	event->def.name = NULL;
	event->gpio = -1;
	event->gpio_idx = -1;
	event->data = NULL;
}

int gpio_debug_event_list_register(struct gpio_debug_event_def *events, int event_count)
{
	struct gpio_debug_data  *data = &debug_data;
	int                     start_index = data->event_count;
	struct gpio_debug_event *new_events;
	int                     new_event_count = data->event_count + event_count;
	int                     i, j;

	mutex_lock(&debug_lock);

	if (data->events)
		for (i = 0; i < data->event_count; i++)
			remove_event_file(&data->events[i]);

	new_events = krealloc(data->events, new_event_count * sizeof(struct gpio_debug_event), GFP_KERNEL);
	if (!new_events) {
		pr_warn("%s: Could not expand for extra events\n", __func__);
		/* If krealloc fails, data->events is unchanged, so just exit */
		return -ENOMEM;
	}
	data->events = new_events;
	for (i = 0; i < data->event_count; i++)
		create_event_file(&data->events[i]);

	data->event_count = new_event_count;

	for (i = 0, j = start_index; (i < event_count) && (j < data->event_count); i++, j++)
		gpio_debug_init_event(data, &events[i], &data->events[j]);

	mutex_unlock(&debug_lock);
	return start_index;
}

void gpio_debug_event_list_unregister(int base, int event_count)
{
	int                    i;
	struct gpio_debug_data *data = &debug_data;

	mutex_lock(&debug_lock);

	for (i = base; (i < (event_count + base)) && (i < data->event_count); i++)
		gpio_debug_destroy_event(&data->events[i]);

	mutex_unlock(&debug_lock);
}

static ssize_t event_list_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	int                    i;
	ssize_t                ret = 0;
	int                    length = 0;
	char                   *buf;
	struct gpio_debug_data *data = file->f_inode->i_private;
	struct device          *dev = &data->pdev->dev;
	char                   headline[] = " gpio event\n";

	mutex_lock(&debug_lock);

	length += strlen(headline);

	for (i = 0; i < data->event_count; i++)
		if (data->events[i].def.name)
			length += strlen(data->events[i].def.name) + 7;
	length++; /* Reserve space for NULL termination */

	buf = devm_kzalloc(dev, length, GFP_KERNEL);
	buf[0] = '\0';
	snprintf(buf, length, "%s", headline);
	for (i = 0; i < data->event_count; i++)
		if (data->events[i].data) {
			if (data->events[i].gpio_idx >= 0)
				snprintf(buf, length, "%s%5d %s\n", buf, data->events[i].gpio_idx, data->events[i].def.name);
			else
				snprintf(buf, length, "%s      %s\n", buf, data->events[i].def.name);
		}
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, length);
	devm_kfree(dev, buf);

	mutex_unlock(&debug_lock);

	return ret;
}

static const struct file_operations event_list_ops = {
	.read = event_list_read,
};

static ssize_t num_gpios_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	ssize_t                ret = 0;
	struct gpio_debug_data *data = file->f_inode->i_private;
	char                   buf[256];
	int                    pos;

	pos = snprintf(buf, sizeof(buf), "%d\n", data->gpio_count);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	return ret;
}

static const struct file_operations num_gpios_ops = {
	.read = num_gpios_read,
};

static int gpio_debug_probe(struct platform_device *pdev)
{
	struct device_node     *np = pdev->dev.of_node;
	struct device          *dev = &pdev->dev;
	int                    count;
	struct gpio_debug_data *data = &debug_data;
	int                    i, j;

	mutex_lock(&debug_lock);

	count = of_gpio_count(np);
	if (count < 0)
		count = 0;  /* Errors register as no GPIOs available */

	data->gpio_count = count;
	data->gpios = NULL;
	data->pdev = pdev;

	if (count) {
		data->gpios = devm_kzalloc(dev, count * sizeof(int), GFP_KERNEL);
		for (i = 0; i < count; i++) {
			data->gpios[i] = of_get_gpio(np, i);
			dev_info(dev, "GPIO at index %d has number %d\n", i, data->gpios[i]);
			if (gpio_is_valid(data->gpios[i])) {
				char label[256];

				sprintf(label, "debug-gpio-%d", i);
				dev_info(dev, "Requesting GPIO %d index %d with label %s\n", data->gpios[i], i, label);
				if (devm_gpio_request(dev, data->gpios[i], label))
					dev_err(dev, "GPIO [%d] request failed\n", data->gpios[i]);
				gpio_set_value(data->gpios[i], 1);
			} else
				dev_warn(dev, "GPIO at index %d is invalid\n", i);
		}
	}

	data->gpio_debug_dir = debugfs_create_dir("gpio_debug", NULL);
	if (!IS_ERR_OR_NULL(data->gpio_debug_dir)) {
		data->gpio_debug_events_dir = debugfs_create_dir("events", data->gpio_debug_dir);
		if (IS_ERR_OR_NULL(data->gpio_debug_events_dir)) {
			data->gpio_debug_events_dir = NULL;
			dev_err(dev, "Debugfs cannot create subdir\n");
		}
		debugfs_create_file("event_list", 0440, data->gpio_debug_dir, data, &event_list_ops);
		debugfs_create_file("num_gpios", 0440, data->gpio_debug_dir, data, &num_gpios_ops);
	} else {
		data->gpio_debug_dir = NULL;
		dev_warn(dev, "Debugfs is not available, configuration of GPIO debug is not possible\n");
	}

	for (i = 0; i < data->event_count; i++)
		create_event_file(&data->events[i]);

	mutex_unlock(&debug_lock);

	data->event_base = gpio_debug_event_list_register(debug_events_table, ARRAY_SIZE(debug_events_table));

	for (i = 0; i < count; i++) {
		gpio_debug_event_link(&data->events[data->event_base + GPIO_DEBUG_TOGGLE_100], i);
		for (j = 0; j < 100; j++) {
			gpio_debug_event_enter(data->event_base, GPIO_DEBUG_TOGGLE_100);
			gpio_debug_event_exit(data->event_base, GPIO_DEBUG_TOGGLE_100);
		}
	}
	gpio_debug_event_link(&data->events[data->event_base + GPIO_DEBUG_TOGGLE_100], -1);

	for (i = 0; i < count; i++) {
		gpio_debug_event_link(&data->events[data->event_base + GPIO_DEBUG_TOGGLE_200], i);
		for (j = 0; j < 200; j++) {
			gpio_debug_event_enter(data->event_base, GPIO_DEBUG_TOGGLE_200);
			gpio_debug_event_exit(data->event_base, GPIO_DEBUG_TOGGLE_200);
		}
	}
	gpio_debug_event_link(&data->events[data->event_base + GPIO_DEBUG_TOGGLE_200], -1);

	return 0;
}

static int gpio_debug_remove(struct platform_device *pdev)
{
	struct device          *dev = &pdev->dev;
	struct gpio_debug_data *data = &debug_data;

	mutex_lock(&debug_lock);
	debugfs_remove_recursive(data->gpio_debug_dir);
	data->gpio_debug_dir = NULL;
	data->gpio_debug_events_dir = NULL;

	if (data->gpios) {
		int i;

		for (i = 0; i < data->gpio_count; i++)
			if (gpio_is_valid(data->gpios[i]))
				devm_gpio_free(dev, data->gpios[i]);
		devm_kfree(dev, data->gpios);
		data->gpios = NULL;
		data->gpio_count = 0;
	}
	data->pdev = NULL;
	kfree(data->events);
	data->events = NULL;
	data->event_count = 0;
	mutex_unlock(&debug_lock);

	return 0;
}

static const struct of_device_id gpio_debug_match[] = {
	{ .compatible = "samsung,gpio-debug", },
	{},
};
MODULE_DEVICE_TABLE(of, gpio_debug_match);

static struct platform_driver    gpio_debug_driver = {
	.probe          = gpio_debug_probe,
	.remove         = gpio_debug_remove,
	.driver         = {
		.name           = "gpio_debug",
		.of_match_table = gpio_debug_match,
	}
};
module_platform_driver(gpio_debug_driver);

MODULE_DESCRIPTION("GPIO Debug framework");
MODULE_AUTHOR("Samsung Electronics Co., Ltd");
MODULE_LICENSE("GPL and additional rights");
