/*
 *  Universal sensors core class
 *
 *  Author : Ryunkyun Park <ryun.park@samsung.com>
 */

#ifndef __SENSORS_CORE_H__
#define __SENSORS_CORE_H__


#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/input.h>

int sensors_create_symlink(struct input_dev *inputdev);
void sensors_remove_symlink(struct input_dev *inputdev);
int sensors_register(struct device *dev, void *drvdata,
                     struct device_attribute *attributes[], char *name);

int sensors_device_register(struct device **pdev, void *drvdata,
                     struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
                        struct device_attribute *attributes[]);
void destroy_sensor_class(void);

#endif
