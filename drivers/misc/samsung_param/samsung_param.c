//-----------------------------------------------------------------------------
// samsung_param.c
//
// Copyright (C)2012 Michael Brehm
// All Rights Reserved
//-----------------------------------------------------------------------------

#include <linux/init.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <mach/param.h>
#include <mach/sec_switch.h>
#include <linux/switch.h>

MODULE_LICENSE("GPL");

// From propretary param.ko module
extern void (*sec_set_param_value)(int idx, void *value);
extern void (*sec_get_param_value)(int idx, void *value);

// From mach-atlas.c
extern struct switch_dev switch_dock_usb_audio;

// From fsa9480.c
extern int fsa9480_get_dock_status(void);

//-----------------------------------------------------------------------------
// Global Variables

int enable_dock_audio = 0;
EXPORT_SYMBOL(enable_dock_audio);

//-----------------------------------------------------------------------------
// command_line_read
//
// Reads the command line parameter

static ssize_t command_line_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	char 		commandLine[PARAM_STRING_SIZE];			// Command line string
	
	if(sec_get_param_value) {
		
		sec_get_param_value(__CMDLINE, commandLine);
		return sprintf(buf, "%s", commandLine);
	}
	
	else printk("samsung_param: Unable to access Samsung Parameter device.  Is param.ko loaded?\n");
    
	return sprintf(buf, "null");							// Unable to read parameter
}

//-----------------------------------------------------------------------------
// command_line_write

static ssize_t command_line_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char 		commandLine[PARAM_STRING_SIZE];			// Command line string
	
	if(sec_set_param_value) {
		
		// Copy up to PARAM_STRING_SIZE - 1 characters into the local buffer
		if(size >= PARAM_STRING_SIZE) size = PARAM_STRING_SIZE - 1;
		memset(commandLine, 0, sizeof(commandLine));
		strncpy(commandLine, buf, size);
		
		sec_set_param_value(__CMDLINE, commandLine);		// Set the new command line
	}
	
	else printk("samsung_param: Unable to access Samsung Parameter device.  Is param.ko loaded?\n");
    
	return size;
}

//-----------------------------------------------------------------------------
// enable_dock_audio_read
//
// Reads the flag to enable dock audio (custom, used only by libaudio and doesn't
// really enable anything)

static ssize_t enable_dock_audio_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	// Just return the global variable value
	return sprintf(buf, "%d", enable_dock_audio);
}

//-----------------------------------------------------------------------------
// enable_dock_audio_write
//
// Write the flag to enable dock audio (custom, used only by libaudio and doesn't
// really enable anything)

static ssize_t enable_dock_audio_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	// Set to 1 if anything other than "0" was passed in
	enable_dock_audio = ((buf) && (*buf != '0'));
	
	// If the device is currently docked, enable the usb_audio switch 
	if(enable_dock_audio && (fsa9480_get_dock_status() == 1))
		switch_set_state(&switch_dock_usb_audio, 1);
	else switch_set_state(&switch_dock_usb_audio, 0);

	return size;
}

//-----------------------------------------------------------------------------
// reboot_mode_read
//
// Reads the reboot mode parameter and converts into a readable string

static ssize_t reboot_mode_read(struct device *dev, struct device_attribute *attr, char *buf)
{
    int 		mode = REBOOT_MODE_NONE;				// Mode read from parameter block
    
    if(sec_get_param_value) {
		
		sec_get_param_value(__REBOOT_MODE, &mode);
		switch(mode) {
			
			case REBOOT_MODE_NONE: return sprintf(buf, "init\n");
			case REBOOT_MODE_DOWNLOAD: return sprintf(buf, "download\n");
			case REBOOT_MODE_CHARGING: return sprintf(buf, "charger\n");
			case REBOOT_MODE_RECOVERY: return sprintf(buf, "recovery\n");
			
			// I don't know what the difference between these two modes are
			// so I'm not going to support them at all.  Not necessary.
			//
			//case REBOOT_MODE_ARM11_FOTA: return sprintf(buf, "fota\n");
			//case REBOOT_MODE_ARM9_FOTA: return sprintf(buf, "fota\n");
		}
	}
	
	else printk("samsung_param: Unable to access Samsung Parameter device.  Is param.ko loaded?\n");
    
    // Couldn't read or wasn't one of the expected values
	return sprintf(buf, "unknown");
}

//-----------------------------------------------------------------------------
// reboot_mode_write

static ssize_t reboot_mode_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int 		mode = REBOOT_MODE_NONE;				// New reboot mode flag to set
	
	if(sec_set_param_value) {
		
		// Only the first character of the input string is necessary here.  Do it case-insensitive.
		if((buf) && size >= 1) {
			
			switch(buf[0]) {
				
				case 'i': case 'I': mode = REBOOT_MODE_NONE; break;	// I = Init
				case 'd': case 'D': mode = REBOOT_MODE_DOWNLOAD; break;	// D = Download
				case 'b': case 'B': mode = REBOOT_MODE_DOWNLOAD; break;	// B = Bootloader (Download)
				case 'c': case 'C': mode = REBOOT_MODE_CHARGING; break; // C = Charger
				case 'l': case 'L': mode = REBOOT_MODE_CHARGING; break; // L = Low-Power Mode (Charger)
				case 'r': case 'R': mode = REBOOT_MODE_RECOVERY; break; // R = Recovery
				
				// I don't know which of the two modes should be set for FOTA,
				// but since there is no FOTA available, this doesn't matter
				//
				//case 'f': case 'F': mode = REBOOT_MODE_ARM??_FOTA; break;
			}
		}
		
		sec_set_param_value(__REBOOT_MODE, &mode);				// Set the new mode flag
		
	}
	
	else printk("samsung_param: Unable to access Samsung Parameter device.  Is param.ko loaded?\n");
	
	return size;
}

//-----------------------------------------------------------------------------
// version_read
//
// Reads the version parameter

static ssize_t version_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	char 		version[PARAM_STRING_SIZE];			// Command line string
	
	if(sec_get_param_value) {
		
		sec_get_param_value(__VERSION, version);
		return sprintf(buf, "%s", version);
	}
	
	else printk("samsung_param: Unable to access Samsung Parameter device.  Is param.ko loaded?\n");
    
	return sprintf(buf, "null");							// Unable to read parameter
}

//-----------------------------------------------------------------------------
// /sys/class/misc/samsung_param/reboot_mode attribute

static DEVICE_ATTR(reboot_mode, S_IRUGO | S_IWUGO, reboot_mode_read, reboot_mode_write);

//-----------------------------------------------------------------------------
// /sys/class/misc/samsung_param/command_line attribute

static DEVICE_ATTR(command_line, S_IRUGO | S_IWUGO, command_line_read, command_line_write);

//-----------------------------------------------------------------------------
// /sys/class/misc/samsung_param/enable_dock_audio attribute

static DEVICE_ATTR(enable_dock_audio, S_IRUGO | S_IWUGO, enable_dock_audio_read, enable_dock_audio_write);

//-----------------------------------------------------------------------------
// /sys/class/misc/samsung_param/enable_dock_audio attribute

static DEVICE_ATTR(version, S_IRUGO, version_read, NULL);

//-----------------------------------------------------------------------------
// /sys/class/misc/samsung_param device

static struct attribute *samsung_param_attributes[] = {
	
	&dev_attr_command_line.attr,
	&dev_attr_enable_dock_audio.attr,
	&dev_attr_reboot_mode.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group samsung_param_group = { .attrs = samsung_param_attributes, };
static struct miscdevice samsung_param_device = { .minor = MISC_DYNAMIC_MINOR, .name = "samsung_param", };

//-------------------------------------------------------------------------------
// samsung_param_init
//
// Module Init entry point

static int __init samsung_param_init(void)
{
	int 			result;				// Result from function call
	
	printk("samsung_param: %s\n", __FUNCTION__);
	
	// Register the device
	result = misc_register(&samsung_param_device);
	if(result != 0) {
		
		printk("samsung_param: %s: Unable to register misc device %s\n", __FUNCTION__, samsung_param_device.name);
		return 1;
	}
	
	// Attempt to create the device attributes
	result = sysfs_create_group(&samsung_param_device.this_device->kobj, &samsung_param_group);
	if(result < 0) printk("samsung_param: %s: Unable to create sysfs group for device %s\n", __FUNCTION__, samsung_param_device.name);

	return 0;
}

module_init(samsung_param_init);

//-------------------------------------------------------------------------------
// samsung_param_exit
//
// Module Exit entry point

static void __exit samsung_param_exit(void)
{
	printk("samsung_param: %s\n", __FUNCTION__);
}

module_exit(samsung_param_exit);

