/*
 * Copyright 2006-2010, Cypress Semiconductor Corporation.
 * Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/timer.h>

#ifdef CONFIG_GENERIC_BLN
#include <linux/bln.h>
#include <linux/wakelock.h>
#endif

#if defined CONFIG_MACH_VICTORY
#include <mach/gpio.h>
#include <mach/gpio-victory.h>
#endif

#define SCANCODE_MASK		0x07
#define UPDOWN_EVENT_MASK	0x08
#define ESD_STATE_MASK		0x10

#define BACKLIGHT_ON		0x10
#define BACKLIGHT_OFF		0x20

#define OLD_BACKLIGHT_ON	0x1
#define OLD_BACKLIGHT_OFF	0x2

#define FORCE_RESET		1
#define DEVICE_NAME "cypress-touchkey"

#ifdef CONFIG_GENERIC_BLN
// djp952: wakelock for BLN support
static struct wake_lock cypress_power_wakelock;
#endif

// djp952: backlight timer
#define BACKLIGHT_TIMEOUT_DEFAULT 15000
static struct timer_list backlight_timer;
static unsigned int backlight_timeout_msec = BACKLIGHT_TIMEOUT_DEFAULT;

struct cypress_touchkey_devdata {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct touchkey_platform_data *pdata;
	struct early_suspend early_suspend;
	u8 backlight_on;
	u8 backlight_off;
	bool is_dead;
	bool is_powering_on;
	bool has_legacy_keycode;
	bool is_delay_led_on;
};

struct cypress_touchkey_devdata *devdata_led;

static int i2c_touchkey_read_byte(struct cypress_touchkey_devdata *devdata,
					u8 *val)
{
	int ret;
	int retry = 2;

	while (true) {
		ret = i2c_smbus_read_byte(devdata->client);
		if (ret >= 0) {
			*val = ret;
			return 0;
		}

		dev_err(&devdata->client->dev, "i2c read error\n");
		if (!retry--)
			break;
		msleep(10);
	}

	return ret;
}

static int i2c_touchkey_write(struct cypress_touchkey_devdata *devdata, 
						u8 * val, unsigned int len)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 2;

	while (retry--) {
		data[0] = *val;
		msg->addr = devdata->client->addr;
		msg->flags = 0;
		msg->len = len;
		msg->buf = data;
		err = i2c_transfer(devdata->client->adapter, msg, 1);
		if (err >= 0)
			return 0;
		printk(KERN_DEBUG "%s %d i2c transfer error\n", __func__,
		       __LINE__);
		mdelay(10);
	}
	return err;
}

// djp952: dead code
//static int i2c_touchkey_write_byte(struct cypress_touchkey_devdata *devdata,
//					u8 val)
//{
//	int ret;
//	int retry = 2;
//
//	while (true) {
//		ret = i2c_smbus_write_byte(devdata->client, val);
//		if (!ret)
//			return 0;
//
//		dev_err(&devdata->client->dev, "i2c write error\n");
//		if (!retry--)
//			break;
//		msleep(10);
//	}
//
//	return ret;
//}

static ssize_t touch_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret;

	if (strncmp(buf, "1", 1) == 0)
	{
		if (devdata_led->is_powering_on){
			dev_err(dev, "%s: delay led on \n", __func__);
			devdata_led->is_delay_led_on = true;
			return size;
		}
		ret = i2c_touchkey_write(devdata_led, &devdata_led->backlight_on, 1);
		printk("Touch Key led ON\n");
		mod_timer(&backlight_timer, jiffies + msecs_to_jiffies(backlight_timeout_msec));
	}
	else
	{
		if (devdata_led->is_powering_on){
			dev_err(dev, "%s: delay led off skip \n", __func__);
			devdata_led->is_delay_led_on = false;
			return size;
		}
		ret = i2c_touchkey_write(devdata_led, &devdata_led->backlight_off, 1);
		printk("Touch Key led OFF\n");	
		del_timer_sync(&backlight_timer);
	}

	if (ret)
		dev_err(dev, "%s: touchkey led i2c failed\n", __func__);

	return size;
}

static ssize_t touch_control_enable_disable(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	printk("touchkey_enable =1 , disable=0 %c \n", *buf);

	if (strncmp(buf, "0", 1) == 0){
#ifdef CONFIG_GENERIC_BLN
		// Disallow powering off the touchkey controller while a notification is ongoing
		if(!bln_is_ongoing()) {
#endif
		devdata_led->is_powering_on = true;
		disable_irq(devdata_led->client->irq);
		if(devdata_led->pdata->touchkey_onoff)
			devdata_led->pdata->touchkey_onoff(TOUCHKEY_OFF);
#ifdef CONFIG_GENERIC_BLN
		} // if(!bln_is_ongoing())
#endif
	}
	else if(strncmp(buf, "1", 1) == 0){
		devdata_led->is_powering_on = false;
		if(devdata_led->pdata->touchkey_onoff)
		devdata_led->pdata->touchkey_onoff(TOUCHKEY_ON);
		enable_irq(devdata_led->client->irq);
	}
	else
		printk("touchkey_enable_disable: unknown command %c \n", *buf);

	return size;
}

static void all_keys_up(struct cypress_touchkey_devdata *devdata)
{
	int i;

	for (i = 0; i < devdata->pdata->keycode_cnt; i++)
		input_report_key(devdata->input_dev,
						devdata->pdata->keycode[i], 0);

	input_sync(devdata->input_dev);
}

static int recovery_routine(struct cypress_touchkey_devdata *devdata)
{
	int ret = -1;
	int retry = 10;
	u8 data;
	int irq_eint;

	if (unlikely(devdata->is_dead)) {
		dev_err(&devdata->client->dev, "%s: Device is already dead, "
				"skipping recovery\n", __func__);
		return -ENODEV;
	}

	irq_eint = devdata->client->irq;

	all_keys_up(devdata);

	disable_irq_nosync(irq_eint);
	while (retry--) {
	if(devdata->pdata->touchkey_onoff) {
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
		devdata->pdata->touchkey_onoff(TOUCHKEY_ON);
	}
		ret = i2c_touchkey_read_byte(devdata, &data);
		if (!ret) {
			enable_irq(irq_eint);
			goto out;
		}
		dev_err(&devdata->client->dev, "%s: i2c transfer error retry = "
				"%d\n", __func__, retry);
	}
	devdata->is_dead = true;
	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	dev_err(&devdata->client->dev, "%s: touchkey died\n", __func__);
out:
	return ret;
}

extern unsigned int touch_state_val;
extern void TSP_forced_release(void);

static irqreturn_t touchkey_interrupt_thread(int irq, void *touchkey_devdata)
{
	u8 data;
	int i;
	int ret;
	int scancode;
	struct cypress_touchkey_devdata *devdata = touchkey_devdata;

	ret = i2c_touchkey_read_byte(devdata, &data);
	if (ret || (data & ESD_STATE_MASK)) {
		ret = recovery_routine(devdata);
		if (ret) {
			dev_err(&devdata->client->dev, "%s: touchkey recovery "
					"failed!\n", __func__);
			goto err;
		}
	}
/*
	if (devdata->has_legacy_keycode) {
		scancode = (data & SCANCODE_MASK) - 1;
		if (scancode < 0 || scancode >= devdata->pdata->keycode_cnt) {
			dev_err(&devdata->client->dev, "%s: scancode is out of "
				"range\n", __func__);
			goto err;
		}
		input_report_key(devdata->input_dev,
			devdata->pdata->keycode[scancode],
			!(data & UPDOWN_EVENT_MASK));
                printk("Key code A %d\n", devdata->pdata->keycode[scancode]);
	} else {
		for (i = 0; i < devdata->pdata->keycode_cnt; i++)
			input_report_key(devdata->input_dev,
				devdata->pdata->keycode[i],
				!!(data & (1U << i))); */

		
	if (data & UPDOWN_EVENT_MASK) {
		scancode = (data & SCANCODE_MASK) - 1;
		input_report_key(devdata->input_dev,
			devdata->pdata->keycode[scancode], 0);
		input_sync(devdata->input_dev);
		dev_dbg(&devdata->client->dev, "[release] cypress touch key : %d \n",
			devdata->pdata->keycode[scancode]);
		printk("Touch_key=release\n");
		mod_timer(&backlight_timer, jiffies + msecs_to_jiffies(backlight_timeout_msec));
		
	} else {
		if (!touch_state_val) {	
			if (devdata->has_legacy_keycode) {
				scancode = (data & SCANCODE_MASK) - 1;
				if (scancode < 0 || scancode >= devdata->pdata->keycode_cnt) {
					dev_err(&devdata->client->dev, "%s: scancode is out of "
						"range\n", __func__);
					goto err;
				}
				if (scancode == 1)
					TSP_forced_release();
				input_report_key(devdata->input_dev,
					devdata->pdata->keycode[scancode], 1);
				
				dev_dbg(&devdata->client->dev, "[press] cypress touch key : %d \n",
					devdata->pdata->keycode[scancode]);	
				//printk("Touch_key=%d\n",devdata->pdata->keycode[scancode]);
					printk("Touch_key=press\n");
				} else {
				for (i = 0; i < devdata->pdata->keycode_cnt; i++)
				input_report_key(devdata->input_dev,
					devdata->pdata->keycode[i],
					!!(data & (1U << i)));        
				//printk("Key code B %d\n", devdata->pdata->keycode[scancode]);
			}		

		input_sync(devdata->input_dev);
		}
	
	}	
err:
	return IRQ_HANDLED;
}

static irqreturn_t touchkey_interrupt_handler(int irq, void *touchkey_devdata)
{
	struct cypress_touchkey_devdata *devdata = touchkey_devdata;

	if (devdata->is_powering_on) {
		dev_dbg(&devdata->client->dev, "%s: ignoring spurious boot "
					"interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_touchkey_early_suspend(struct early_suspend *h)
{
	int ret; 
	struct cypress_touchkey_devdata *devdata =
		container_of(h, struct cypress_touchkey_devdata, early_suspend);

	devdata->is_powering_on = true;

	if (unlikely(devdata->is_dead))
		return;

	disable_irq(devdata->client->irq);
	
#ifdef CONFIG_GENERIC_BLN
	// Disallow powering off the touchkey controller while a notification is ongoing
	if(!bln_is_ongoing()) {
#endif
	
	del_timer_sync(&backlight_timer);
	ret = i2c_touchkey_write(devdata, &devdata->backlight_on, 0);
	dev_err(&devdata->client->dev,"%s: Touch Key led ON ret = %d\n",__func__, ret);

	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
#ifdef CONFIG_GENERIC_BLN
	} // if(!bln_is_ongoing())
#endif

	all_keys_up(devdata);
}

static void cypress_touchkey_early_resume(struct early_suspend *h)
{
	int ret;
	struct cypress_touchkey_devdata *devdata =
		container_of(h, struct cypress_touchkey_devdata, early_suspend);
	
	msleep(1);

	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_ON);

	#if 0
	if (i2c_touchkey_write_byte(devdata, devdata->backlight_on)) {
		devdata->is_dead = true;
	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
		dev_err(&devdata->client->dev, "%s: touch keypad not responding"
				" to commands, disabling\n", __func__);
		return;
	}
	#endif

	if (devdata->is_delay_led_on){
		ret = i2c_touchkey_write(devdata, &devdata->backlight_on, 1);
		dev_err(&devdata->client->dev,"%s: Touch Key led ON ret = %d\n",__func__, ret);
		mod_timer(&backlight_timer, jiffies + msecs_to_jiffies(backlight_timeout_msec));
#if FORCE_RESET
		if(ret != 0)
		{
			devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
			devdata->pdata->touchkey_onoff(TOUCHKEY_ON);
			printk("%s: Touch Key Force Reset",__func__);
			ret = i2c_touchkey_write(devdata, &devdata->backlight_on, 1);
			dev_err(&devdata->client->dev,"%s: Touch Key led ON ret = %d\n",__func__, ret);
			mod_timer(&backlight_timer, jiffies + msecs_to_jiffies(backlight_timeout_msec));
		}
#endif
	}
	devdata->is_delay_led_on = false;
	devdata->is_dead = false;
	devdata->is_powering_on = false;

	enable_irq(devdata->client->irq);

#ifdef CONFIG_GENERIC_BLN
	// Deactivate the BLN wake lock
	if(wake_lock_active(&cypress_power_wakelock)) {
		wake_unlock(&cypress_power_wakelock);
	}
#endif // CONFIG_GENERIC_BLN
	
}
#endif

static DEVICE_ATTR(brightness, 0660, NULL,touch_led_control);
static DEVICE_ATTR(enable_disable, 0660, NULL,touch_control_enable_disable);

extern struct class *sec_class;
struct device *ts_key_dev;

#ifdef CONFIG_GENERIC_BLN

// djp952: BLN implementation
static int cypress_enable_touchkey_backlights(int led_mask)
{
	// echo 1 > /sys/class/sec/t_key/brightness
	return touch_led_control(&devdata_led->client->dev, &dev_attr_brightness, "1", 1);
}

static int cypress_disable_touchkey_backlights(int led_mask)
{
	// echo 0 > /sys/class/sec/t_key/brightness
	return touch_led_control(&devdata_led->client->dev, &dev_attr_brightness, "0", 1);
}

static int cypress_power_on_touchkey_controller(void)
{
	// Activate the BLN wake lock
	if(!wake_lock_active(&cypress_power_wakelock)) wake_lock(&cypress_power_wakelock);
	
	// Directly manipulate the device as touch_control_enable_disable would, except
	// don't enable the IRQ, which would activate the keys and allow them to wake the device
	devdata_led->is_powering_on = false;
	if(devdata_led->pdata->touchkey_onoff) devdata_led->pdata->touchkey_onoff(TOUCHKEY_ON);
	
	return 0;
}

static int cypress_power_off_touchkey_controller(void)
{
	// Directly manipulate the device as touch_control_enable_disable would, except 
	// don't disable the IRQ
	devdata_led->is_powering_on = true;
	if(devdata_led->pdata->touchkey_onoff) devdata_led->pdata->touchkey_onoff(TOUCHKEY_OFF);
	
	// Deactivate the BLN wake lock
	if(wake_lock_active(&cypress_power_wakelock)) wake_unlock(&cypress_power_wakelock);
	
	return 0;
}

static struct bln_implementation cypress_touchkey_bln = 
{
	.enable = cypress_enable_touchkey_backlights,
	.disable = cypress_disable_touchkey_backlights,
	.power_on = cypress_power_on_touchkey_controller,
	.power_off = cypress_power_off_touchkey_controller,
	.led_count = 1
};

#endif	// CONFIG_GENERIC_BLN

//-----------------------------------------------------------------------------
// BACKLIGHT TIMER

// backlight_timer_work_func
//
// Work function that turns off the touchkey backlight
static void backlight_timer_work_func(struct work_struct* work)
{
	touch_led_control(&devdata_led->client->dev, &dev_attr_brightness, "0", 1);	
}

// backlight_timer_work
//
// Workqueue to turn off the touchkey backlight
static DECLARE_WORK(backlight_timer_work, backlight_timer_work_func);

// backlight_timer_handler
//
// Invoked when the period of the backlight timer has expired
static void backlight_timer_handler(unsigned long unused)
{
	// Delete the timer
	del_timer_sync(&backlight_timer);

#ifdef CONFIG_GENERIC_BLN
	if(!bln_is_ongoing()) {
#endif

	// Schedule the work to turn off the touchkey backlight
	schedule_work(&backlight_timer_work);

#ifdef CONFIG_GENERIC_BLN
	}
#endif
}

// backlight_timeout_read
//
// Returns the currently set backlight timer period in milliseconds
static ssize_t backlight_timeout_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", backlight_timeout_msec);
}

// backlight_timeout_write
//
// Sets the backlight timer period in milliseconds.  Range = 500-30000
static ssize_t backlight_timeout_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data = BACKLIGHT_TIMEOUT_DEFAULT;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}
	
	if(data < 500) data = 500;			// .5 seconds minimum
	if(data > 30000) data = 30000;		// 30 seconds maximum

	backlight_timeout_msec = data;

	return size;
}

// backlight_timeout device attribute
//
// /sys/class/sec/t_key/backlight_timeout
static DEVICE_ATTR(backlight_timeout, S_IRUGO | S_IWUGO, backlight_timeout_read, backlight_timeout_write);

//-----------------------------------------------------------------------------

static int cypress_touchkey_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct input_dev *input_dev;
	struct cypress_touchkey_devdata *devdata;
	u8 data[3];
	int err;
	int cnt;

	if (!dev->platform_data) {
		dev_err(dev, "%s: Platform data is NULL\n", __func__);
		return -EINVAL;
	}

	devdata = kzalloc(sizeof(*devdata), GFP_KERNEL);
	devdata_led = devdata;
	if (devdata == NULL) {
		dev_err(dev, "%s: failed to create our state\n", __func__);
		return -ENODEV;
	}

	devdata->client = client;
	i2c_set_clientdata(client, devdata);

	devdata->pdata = client->dev.platform_data;
	if (!devdata->pdata->keycode) {
		dev_err(dev, "%s: Invalid platform data\n", __func__);
		err = -EINVAL;
		goto err_null_keycodes;
	}

	strlcpy(devdata->client->name, DEVICE_NAME, I2C_NAME_SIZE);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		goto err_input_alloc_dev;
	}

	devdata->input_dev = input_dev;
	dev_set_drvdata(&input_dev->dev, devdata);
	input_dev->name = DEVICE_NAME;
	input_dev->id.bustype = BUS_HOST;

	for (cnt = 0; cnt < devdata->pdata->keycode_cnt; cnt++)
		input_set_capability(input_dev, EV_KEY,
					devdata->pdata->keycode[cnt]);

	err = input_register_device(input_dev);
	if (err)
		goto err_input_reg_dev;

	devdata->is_powering_on = true;
	devdata->is_delay_led_on = false;

	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_ON);	

	msleep(100);
	err = i2c_master_recv(client, data, sizeof(data));
	if (err < sizeof(data)) {
		if (err >= 0)
			err = -EIO;
		dev_err(dev, "%s: error reading hardware version\n", __func__);
		goto err_read;
	}

	printk("touch_key hardware rev1 = %#02x, rev2 = %#02x\n", data[1], data[2]);
	
	// djp952: The SCH-I500 uses the old/legacy keycodes and command bytes
	devdata->backlight_on = OLD_BACKLIGHT_ON;
	devdata->backlight_off = OLD_BACKLIGHT_OFF;
	devdata->has_legacy_keycode = 1;

	if (sec_class == NULL)
		   sec_class = class_create(THIS_MODULE, "sec");
	
		   if (IS_ERR(sec_class))
				   pr_err("Failed to create class(sec)!\n");
	
	   ts_key_dev = device_create(sec_class, NULL, 0, NULL, "t_key");
	   if (IS_ERR(ts_key_dev))
		   pr_err("Failed to create device(ts)!\n");
	   if (device_create_file(ts_key_dev, &dev_attr_brightness) < 0)
		   pr_err("Failed to create device file for Touch key!\n");
	    if (device_create_file(ts_key_dev, &dev_attr_enable_disable) < 0)
		pr_err("Failed to create device file for Touch key_enable_disable!\n");
	   
	   // djp952: add the new dev_attr for the configurable backlight timeout
	   if(device_create_file(ts_key_dev, &dev_attr_backlight_timeout) < 0)
		   pr_err("Failed to create device file for Touch Key backlight_timeout\n");
	

#if 0
	err = i2c_touchkey_write_byte(devdata, devdata->backlight_on);
	if (err) {
		dev_err(dev, "%s: touch keypad backlight on failed\n",
				__func__);
		goto err_backlight_on;
	}
#endif
	if (request_threaded_irq(client->irq, touchkey_interrupt_handler,
				touchkey_interrupt_thread, IRQF_TRIGGER_FALLING,
				DEVICE_NAME, devdata)) {
		dev_err(dev, "%s: Can't allocate irq.\n", __func__);
		goto err_req_irq;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	devdata->early_suspend.suspend = cypress_touchkey_early_suspend;
	devdata->early_suspend.resume = cypress_touchkey_early_resume;
#endif
	register_early_suspend(&devdata->early_suspend);
	
#ifdef CONFIG_GENERIC_BLN
	wake_lock_init(&cypress_power_wakelock, WAKE_LOCK_SUSPEND, "cypress_power_wakelock");
#endif
	
	devdata->is_powering_on = false;

	return 0;

err_req_irq:
//err_backlight_on:
err_read:
	
	if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	input_unregister_device(input_dev);
	goto err_input_alloc_dev;
err_input_reg_dev:
	input_free_device(input_dev);
err_input_alloc_dev:
err_null_keycodes:
	kfree(devdata);
	return err;
}

static int __devexit i2c_touchkey_remove(struct i2c_client *client)
{
	struct cypress_touchkey_devdata *devdata = i2c_get_clientdata(client);

	unregister_early_suspend(&devdata->early_suspend);
	/* If the device is dead IRQs are disabled, we need to rebalance them */
	if (unlikely(devdata->is_dead))
		enable_irq(client->irq);
	else if(devdata->pdata->touchkey_onoff)
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	free_irq(client->irq, devdata);
	all_keys_up(devdata);
	input_unregister_device(devdata->input_dev);
	kfree(devdata);
	return 0;
}

static const struct i2c_device_id cypress_touchkey_id[] = {
	{ CYPRESS_TOUCHKEY_DEV_NAME, 0 },
};

MODULE_DEVICE_TABLE(i2c, cypress_touchkey_id);

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "cypress_touchkey_driver",
	},
	.id_table = cypress_touchkey_id,
	.probe = cypress_touchkey_probe,
	.remove = __devexit_p(i2c_touchkey_remove),
};

static int __init touchkey_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&touchkey_i2c_driver);
	if (ret)
		pr_err("%s: cypress touch keypad registration failed. (%d)\n",
				__func__, ret);

#ifdef CONFIG_GENERIC_BLN
	register_bln_implementation(&cypress_touchkey_bln);
#endif
	
	// djp952: Initialize backlight timer
	init_timer(&backlight_timer);
	backlight_timer.function = backlight_timer_handler;
	backlight_timer.expires = 0;
	backlight_timer.data = 0;
	
	return ret;
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&touchkey_i2c_driver);
	
#ifdef CONFIG_GENERIC_BLN
	wake_lock_destroy(&cypress_power_wakelock);
#endif
	
}

late_initcall(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("cypress touch keypad");
