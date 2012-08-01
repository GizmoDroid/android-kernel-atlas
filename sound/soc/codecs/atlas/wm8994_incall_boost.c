#include <sound/soc.h>
#include <linux/miscdevice.h>
#include "wm8994_def.h"
#include "wm8994_incall_boost.h"

//-----------------------------------------------------------------------------

static ssize_t incall_boost_rcv_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", incall_boost_rcv >> WM8994_AIF2DAC_BOOST_SHIFT);
}

static ssize_t incall_boost_rcv_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data = 0;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}
	
	if(data > 3) data = 3;
	incall_boost_rcv = (data << WM8994_AIF2DAC_BOOST_SHIFT);

	return size;
}

//-----------------------------------------------------------------------------

static ssize_t incall_boost_bt_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", incall_boost_bt >> WM8994_AIF2DAC_BOOST_SHIFT);
}

static ssize_t incall_boost_bt_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data = 0;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}
	
	if(data > 3) data = 3;
	incall_boost_bt = (data << WM8994_AIF2DAC_BOOST_SHIFT);

	return size;
}

//-----------------------------------------------------------------------------

static ssize_t incall_boost_spk_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", incall_boost_spk >> WM8994_AIF2DAC_BOOST_SHIFT);
}

static ssize_t incall_boost_spk_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data = 0;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}
	
	if(data > 3) data = 3;
	incall_boost_spk = (data << WM8994_AIF2DAC_BOOST_SHIFT);

	return size;
}

//-----------------------------------------------------------------------------

static ssize_t incall_boost_hp_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", incall_boost_hp >> WM8994_AIF2DAC_BOOST_SHIFT);
}

static ssize_t incall_boost_hp_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data = 0;

	if (sscanf(buf, "%u\n", &data) != 1) {
		pr_info("%s: input error\n", __FUNCTION__);
		return size;
	}
	
	if(data > 3) data = 3;
	incall_boost_hp = (data << WM8994_AIF2DAC_BOOST_SHIFT);

	return size;
}

//-----------------------------------------------------------------------------

static DEVICE_ATTR(incall_boost_rcv, S_IRUGO | S_IWUGO, incall_boost_rcv_read, incall_boost_rcv_write);
static DEVICE_ATTR(incall_boost_bt, S_IRUGO | S_IWUGO, incall_boost_bt_read, incall_boost_bt_write);
static DEVICE_ATTR(incall_boost_spk, S_IRUGO | S_IWUGO, incall_boost_spk_read, incall_boost_spk_write);
static DEVICE_ATTR(incall_boost_hp, S_IRUGO | S_IWUGO, incall_boost_hp_read, incall_boost_hp_write);

static struct attribute *incall_boost_attributes[] = {
	&dev_attr_incall_boost_rcv,
	&dev_attr_incall_boost_bt,
	&dev_attr_incall_boost_spk,
	&dev_attr_incall_boost_hp,
	NULL
};

static struct attribute_group incall_boost_group = {
	.attrs = incall_boost_attributes,
};

static struct miscdevice incall_boost_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "incall_boost",
};

//-----------------------------------------------------------------------------

void incall_boost_hook_wm8994_pcm_probe(struct snd_soc_codec *codec_)
{
	misc_register(&incall_boost_device);
	if (sysfs_create_group(&incall_boost_device.this_device->kobj, &incall_boost_group) < 0) {
		printk("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n", incall_boost_device.name);
	}
}


