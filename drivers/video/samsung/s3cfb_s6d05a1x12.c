/*
 * s6e63m0 AMOLED Panel Driver for the Samsung Universal board
 *
 * Derived from drivers/video/omap/lcd-apollon.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/tl2796.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-fb.h>
#include <linux/earlysuspend.h>
 
#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define DEFMASK		0xFF00
#define DIM_BL	20
#define MIN_BL	30
#define MAX_BL	255
#define MAX_GAMMA_VALUE	24

extern struct class *sec_class;

/*********** for debug **********************************************************/
#if 0
#define gprintk(fmt, x... ) printk("%s(%d): " fmt, __FUNCTION__ , __LINE__ , ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#ifdef CONFIG_FB_S3C_MDNIE
extern void init_mdnie_class(void);
#endif

#if defined(CONFIG_MACH_AEGIS) || defined(CONFIG_MACH_VIPER)
int call_for_lcd = 0;
#endif

struct s5p_lcd {
	int ldi_enable;
	int bl;
	int acl_enable;
	int cur_acl;
	int on_19gamma;
	const struct tl2796_gamma_adj_points *gamma_adj_points;
	struct mutex	lock;
	struct device *dev;
	struct spi_device *g_spi;
	struct s5p_panel_data	*data;
	struct backlight_device *bl_dev;
	struct lcd_device *lcd_dev;
	struct class *acl_class;
	struct device *switch_aclset_dev;
	struct class *gammaset_class;
	struct device *switch_gammaset_dev;
	struct device *sec_lcdtype_dev;
	struct early_suspend    early_suspend;
};

#define DATA_ONLY 0xFF
#define COMMAND_ONLY	0xFE

const unsigned short SEQ_STAND_BY_ON[] = {
	0x10, COMMAND_ONLY,
	
	ENDDEF, 0x0000
};

const unsigned short SEQ_DISPLAY_ON[] = {
	0x29, COMMAND_ONLY,
	
	ENDDEF, 0x0000
};

const unsigned short SEQ_STAND_BY_OFF[] = {
	0x11, COMMAND_ONLY,

	ENDDEF, 0x0000
};

const unsigned short SEQ_LEVEL2_COMMAND_ACCESS[] = {
	0xF0, 0x5A,
	DATA_ONLY, 0x5A,

	0xF1, 0x5A,
	DATA_ONLY, 0x5A,
 
	ENDDEF, 0x0000
};

const unsigned short SEQ_POWER_SETTING_SEQ[] = {
	0xF2, 0x3B,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x04,

	0xF4, 0x08,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x70,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x70,
	DATA_ONLY, 0x03,

	0xF5,0x00,
	DATA_ONLY, 0x68,
	DATA_ONLY, 0x70,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x68,
	DATA_ONLY, 0x70,
 
	ENDDEF, 0x0000
};

const unsigned short SEQ_INIT_SEQ[] = {
	0x3A, 0x77,

	0x36, 0xD0,		

	0x2A, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x3F,

	0x2B, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,

	0xF6, 0x03,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x00,

	0xF7, 0x48,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xF0,
	DATA_ONLY, 0x12,
	DATA_ONLY, 0x00,
 	
	0xF8, 0x11,
	DATA_ONLY, 0x00,
	
	ENDDEF, 0x0000
};

const unsigned short SEQ_GAMMA_SETTING[] = {
	0xF9, 0x14,

	0xFA, 0x01,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x24,
	DATA_ONLY, 0x27,
	DATA_ONLY, 0x33,
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x0E,
	DATA_ONLY, 0x1C,
	DATA_ONLY, 0x22,
	DATA_ONLY, 0x29,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

	0xFB, 0x0E,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x29,
	DATA_ONLY, 0x22,
	DATA_ONLY, 0x1C,
	DATA_ONLY, 0x0E,
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x33,
	DATA_ONLY, 0x27,
	DATA_ONLY, 0x24,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

	0xF9, 0x12,

	0xFA, 0x11,
	DATA_ONLY, 0x19,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x2A,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0x34,
	DATA_ONLY, 0x38,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

	0xFB, 0x17,
	DATA_ONLY, 0x11,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x38,
	DATA_ONLY, 0x34,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0x2A,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

	0xF9, 0x11,

	0xFA, 0x38,
	DATA_ONLY, 0x16,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x0B,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x35,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x3E,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

	0xFB, 0x14,
	DATA_ONLY, 0x38,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x36,
	DATA_ONLY, 0x3E,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x35,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x0B,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,

 	ENDDEF, 0x0000
};

const unsigned short SEQ_BACKLIGHT[] = {
	0x53, 0x2C,

	0xC3,0x00,
	DATA_ONLY, 0x2F,
	DATA_ONLY, 0x14,

	0x51,0x80,
	
	ENDDEF, 0x0000
};

static unsigned short SEQ_BRIGHTNESS_SETTING[] = {
	0x051, 0x17f,
	ENDDEF, 0x0000
};


static char lcd_brightness = 0;

static int s6e63m0_spi_write_driver(struct s5p_lcd *lcd, u16 reg)
{
	u16 buf[1];
	int ret;
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len	= 2,
		.tx_buf	= buf,
	};

	buf[0] = reg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(lcd->g_spi, &msg);

	if (ret < 0)
		pr_err("%s error\n", __func__);

	return ret ;
}

static inline void lcd_cs_value(int level)
{
	gpio_set_value(GPIO_DISPLAY_CS, level);
}

static inline void lcd_clk_value(int level)
{
	gpio_set_value(GPIO_DISPLAY_CLK, level);
}

static inline void lcd_si_value(int level)
{
	gpio_set_value(GPIO_DISPLAY_SI, level);
}

static void s6e63m0_c110_spi_write_byte(unsigned char address, unsigned char command)
{
	int     j;
	unsigned short data;

	data = (address << 8) + command;

	lcd_cs_value(1);
	lcd_si_value(1);
	lcd_clk_value(1);
	udelay(1);

	lcd_cs_value(0);
	udelay(1);

	for (j = 8; j >= 0; j--)
	{
		lcd_clk_value(0);

		/* data high or low */
		if ((data >> j) & 0x0001)
			lcd_si_value(1);
		else
			lcd_si_value(0);

		udelay(1);

		lcd_clk_value(1);
		udelay(1);
	}

	lcd_cs_value(1);
	udelay(1);
}

static void s6e63m0_spi_write(unsigned char address, unsigned char command)
{
	if (address != DATA_ONLY)
		s6e63m0_c110_spi_write_byte(0x0, address);

	if (command != COMMAND_ONLY)
		s6e63m0_c110_spi_write_byte(0x1, command);
}

void s6e63m0_panel_send_sequence(const unsigned short *wbuf)
{
	int i = 0;

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC)
			s6e63m0_spi_write(wbuf[i], wbuf[i+1]);
		else
			udelay(wbuf[i+1]*1000);
		i += 2;
	}
}
#if 0
static void s6e63m0_panel_send_sequence(struct s5p_lcd *lcd,
	const u16 *wbuf)
{
	int i = 0;

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC) {
			s6e63m0_spi_write_driver(lcd, wbuf[i]);
			i += 1;
		} else {
			msleep(wbuf[i+1]);
			i += 2;
		}
	}
}
#endif
static int get_gamma_value_from_bl(int bl)
{
	int gamma_value = 0;
	int gamma_val_x10 = 0;

	if (bl >= MIN_BL)		{
		gamma_val_x10 = 10*(MAX_GAMMA_VALUE-1)*bl/(MAX_BL-MIN_BL) + (10 - 10*(MAX_GAMMA_VALUE-1)*(MIN_BL)/(MAX_BL-MIN_BL)) ;
		gamma_value = (gamma_val_x10+5)/10;
	} else {
		gamma_value = 0;
	}

	return gamma_value;
}

#ifdef CONFIG_FB_S3C_TL2796_ACL
static void update_acl(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;
	int gamma_value;

	gamma_value = get_gamma_value_from_bl(lcd->bl);

	if (lcd->acl_enable) {
		if ((lcd->cur_acl == 0) && (gamma_value != 1)) {
			s6e63m0_panel_send_sequence(lcd, pdata->acl_init);
			msleep(20);
		}

		switch (gamma_value) {
		case 1:
			if (lcd->cur_acl != 0) {
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[0]);
				lcd->cur_acl = 0;
				gprintk(" ACL_cutoff_set Percentage : 0!!\n");
			}
			break;
		case 2 ... 12:
			if (lcd->cur_acl != 40)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[9]);
				lcd->cur_acl = 40;
				gprintk(" ACL_cutoff_set Percentage : 40!!\n");
			}
			break;
		case 13:
			if (lcd->cur_acl != 43)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[16]);
				lcd->cur_acl = 43;
				gprintk(" ACL_cutoff_set Percentage : 43!!\n");
			}
			break;
		case 14:
			if (lcd->cur_acl != 45)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[10]);
				lcd->cur_acl = 45;
				gprintk(" ACL_cutoff_set Percentage : 45!!\n");
			}
			break;
		case 15:
			if (lcd->cur_acl != 47)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[11]);
				lcd->cur_acl = 47;
				gprintk(" ACL_cutoff_set Percentage : 47!!\n");
			}
			break;
		case 16:
			if (lcd->cur_acl != 48)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[12]);
				lcd->cur_acl = 48;
				gprintk(" ACL_cutoff_set Percentage : 48!!\n");
			}
			break;
		default:
			if (lcd->cur_acl != 50)	{
				s6e63m0_panel_send_sequence(lcd, pdata->acl_table[13]);
				lcd->cur_acl = 50;
				gprintk(" ACL_cutoff_set Percentage : 50!!\n");
			}
		}
	} else	{
		s6e63m0_panel_send_sequence(lcd, pdata->acl_table[0]);
		lcd->cur_acl  = 0;
		gprintk(" ACL_cutoff_set Percentage : 0!!\n");
	}

}
#endif

static void update_brightness(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;
	int bl_level = lcd->bl;

	printk("bl:%d\n", bl_level);	

	SEQ_BRIGHTNESS_SETTING[1]=0X00;
	s6e63m0_panel_send_sequence(SEQ_BRIGHTNESS_SETTING);
	udelay(100); 
	
	bl_level = (bl_level * 189)/255;  //MBjclee
	SEQ_BRIGHTNESS_SETTING[1]=bl_level;
	s6e63m0_panel_send_sequence(SEQ_BRIGHTNESS_SETTING);

	lcd_brightness = bl_level;
}


static void tl2796_ldi_enable(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;

	printk("tl2796_ldi_enable\n");
	mutex_lock(&lcd->lock);

	s6e63m0_panel_send_sequence(SEQ_LEVEL2_COMMAND_ACCESS);
	s6e63m0_panel_send_sequence(SEQ_POWER_SETTING_SEQ);
	s6e63m0_panel_send_sequence(SEQ_INIT_SEQ);
	s6e63m0_panel_send_sequence(SEQ_GAMMA_SETTING);
	s6e63m0_panel_send_sequence(SEQ_BACKLIGHT);
 	s6e63m0_panel_send_sequence(SEQ_STAND_BY_OFF);
	mdelay(130);
	s6e63m0_panel_send_sequence(SEQ_DISPLAY_ON);
 	mdelay(45);	

	lcd->ldi_enable = 1;

	mutex_unlock(&lcd->lock);
}

static void tl2796_ldi_disable(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;

	mutex_lock(&lcd->lock);

	lcd->ldi_enable = 0;
	s6e63m0_panel_send_sequence(SEQ_STAND_BY_ON);
		
//	s6e63m0_panel_send_sequence(lcd, pdata->standby_on);

	mutex_unlock(&lcd->lock);
}


static int s5p_lcd_set_power(struct lcd_device *ld, int power)
{
	struct s5p_lcd *lcd = lcd_get_data(ld);
	struct s5p_panel_data *pdata = lcd->data;

	printk(KERN_DEBUG "s5p_lcd_set_power is called: %d", power);

	if (power)
		s6e63m0_panel_send_sequence(SEQ_DISPLAY_ON);
	else
		s6e63m0_panel_send_sequence(SEQ_STAND_BY_ON);

	return 0;
}

static int s5p_lcd_check_fb(struct lcd_device *lcddev, struct fb_info *fi)
{
	return 0;
}

struct lcd_ops s5p_lcd_ops = {
	.set_power = s5p_lcd_set_power,
	.check_fb = s5p_lcd_check_fb,
};

static ssize_t gammaset_file_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s5p_lcd *lcd = dev_get_drvdata(dev);

	gprintk("called %s\n", __func__);

	return sprintf(buf, "%u\n", lcd->bl);
}
static ssize_t gammaset_file_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct s5p_lcd *lcd = dev_get_drvdata(dev);
	int value;

	sscanf(buf, "%d", &value);

	if ((lcd->ldi_enable) && ((value == 0) || (value == 1))) {
		printk("[gamma set] in gammaset_file_cmd_store, input value = %d\n", value);
		if (value != lcd->on_19gamma)	{
			lcd->on_19gamma = value;
			update_brightness(lcd);
		}
	}

	return size;
}

static DEVICE_ATTR(gammaset_file_cmd, 0664, gammaset_file_cmd_show, gammaset_file_cmd_store);

static ssize_t aclset_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct s5p_lcd *lcd = dev_get_drvdata(dev);
	gprintk("called %s\n", __func__);

	return sprintf(buf, "%u\n", lcd->acl_enable);
}
static ssize_t aclset_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct s5p_lcd *lcd = dev_get_drvdata(dev);
	int value;

	sscanf(buf, "%d", &value);

	if ((lcd->ldi_enable) && ((value == 0) || (value == 1))) {
		printk(KERN_INFO "[acl set] in aclset_file_cmd_store, input value = %d\n", value);
		if (lcd->acl_enable != value) {
			lcd->acl_enable = value;
#ifdef CONFIG_FB_S3C_TL2796_ACL
			update_acl(lcd);
#endif
		}
	}

	return size;
}

static DEVICE_ATTR(aclset_file_cmd, 0664, aclset_file_cmd_show, aclset_file_cmd_store);

static ssize_t lcdtype_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "SMD_AMS397GE03\n");
}

static ssize_t lcdtype_file_cmd_store(
        struct device *dev, struct device_attribute *attr,
        const char *buf, size_t size)
{
    return size;
}
static DEVICE_ATTR(lcdtype_file_cmd, 0664, lcdtype_file_cmd_show, lcdtype_file_cmd_store);

#if defined(CONFIG_MACH_AEGIS) || defined(CONFIG_MACH_VIPER)
static ssize_t call_lcd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n",call_for_lcd);
}

static ssize_t call_lcd_store(
        struct device *dev, struct device_attribute *attr,
        const char *buf, size_t size)
{
	sscanf(buf, "%d", &call_for_lcd);

    return call_for_lcd;
}
static DEVICE_ATTR(call_lcd, 0664, call_lcd_show, call_lcd_store);
#endif

static int s5p_bl_update_status(struct backlight_device *bd)
{
	struct s5p_lcd *lcd = bl_get_data(bd);
	int bl = bd->props.brightness;

	pr_debug("\nupdate status brightness %d\n",
				bd->props.brightness);

	if (bl < 0 || bl > 255)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	lcd->bl = bl;

	if (lcd->ldi_enable) {
		pr_debug("\n bl :%d\n", bl);
		update_brightness(lcd);
	}

	mutex_unlock(&lcd->lock);

	return 0;
}


static int s5p_bl_get_brightness(struct backlight_device *bd)
{
	struct s5p_lcd *lcd = bl_get_data(bd);

	printk(KERN_DEBUG "\n reading brightness\n");

	return lcd->bl;
}

const struct backlight_ops s5p_bl_ops = {
	.update_status = s5p_bl_update_status,
	.get_brightness = s5p_bl_get_brightness,
};

void tl2796_early_suspend(struct early_suspend *h)
{
	struct s5p_lcd *lcd = container_of(h, struct s5p_lcd,
								early_suspend);

	printk("%s\n",__FUNCTION__);
	tl2796_ldi_disable(lcd);

	return ;
}
void tl2796_late_resume(struct early_suspend *h)
{
	struct s5p_lcd *lcd = container_of(h, struct s5p_lcd,
								early_suspend);

	printk("%s\n",__FUNCTION__);
	tl2796_ldi_enable(lcd);

	return ;
}
static int __devinit tl2796_probe(struct spi_device *spi)
{
	struct s5p_lcd *lcd;
	int ret;

	lcd = kzalloc(sizeof(struct s5p_lcd), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	mutex_init(&lcd->lock);

	spi->bits_per_word = 9;
	if (spi_setup(spi)) {
		pr_err("failed to setup spi\n");
		ret = -EINVAL;
		goto err_setup;
	}

	lcd->g_spi = spi;
	lcd->dev = &spi->dev;
	lcd->bl = 255;

	if (!spi->dev.platform_data) {
		dev_err(lcd->dev, "failed to get platform data\n");
		ret = -EINVAL;
		goto err_setup;
	}
	lcd->data = (struct s5p_panel_data *)spi->dev.platform_data;

	if (!lcd->data->gamma19_table || !lcd->data->gamma19_table ||
		!lcd->data->seq_display_set || !lcd->data->seq_etc_set ||
		!lcd->data->display_on || !lcd->data->display_off ||
		!lcd->data->standby_on || !lcd->data->standby_off ||
		!lcd->data->acl_init || !lcd->data->acl_table ||
		!lcd->data->gamma_update) {
		dev_err(lcd->dev, "Invalid platform data\n");
		ret = -EINVAL;
		goto err_setup;
	}

	lcd->bl_dev = backlight_device_register("s5p_bl",
			&spi->dev, lcd, &s5p_bl_ops, NULL);
	if (!lcd->bl_dev) {
		dev_err(lcd->dev, "failed to register backlight\n");
		ret = -EINVAL;
		goto err_setup;
	}

	lcd->bl_dev->props.max_brightness = 255;

	lcd->lcd_dev = lcd_device_register("s5p_lcd",
			&spi->dev, lcd, &s5p_lcd_ops);
	if (!lcd->lcd_dev) {
		dev_err(lcd->dev, "failed to register lcd\n");
		ret = -EINVAL;
		goto err_setup_lcd;
	}

	lcd->gammaset_class = class_create(THIS_MODULE, "gammaset");
	if (IS_ERR(lcd->gammaset_class))
		pr_err("Failed to create class(gammaset_class)!\n");

	lcd->switch_gammaset_dev = device_create(lcd->gammaset_class, &spi->dev, 0, lcd, "switch_gammaset");
	if (!lcd->switch_gammaset_dev) {
		dev_err(lcd->dev, "failed to register switch_gammaset_dev\n");
		ret = -EINVAL;
		goto err_setup_gammaset;
	}

	if (device_create_file(lcd->switch_gammaset_dev, &dev_attr_gammaset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_gammaset_file_cmd.attr.name);

	lcd->acl_enable = 0;
	lcd->cur_acl = 0;
	lcd->ldi_enable = 1;

	lcd->acl_class = class_create(THIS_MODULE, "aclset");
	if (IS_ERR(lcd->acl_class))
		pr_err("Failed to create class(acl_class)!\n");

	lcd->switch_aclset_dev = device_create(lcd->acl_class, &spi->dev, 0, lcd, "switch_aclset");
	if (IS_ERR(lcd->switch_aclset_dev))
		pr_err("Failed to create device(switch_aclset_dev)!\n");

	if (device_create_file(lcd->switch_aclset_dev, &dev_attr_aclset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_aclset_file_cmd.attr.name);

	 if (sec_class == NULL)
	 	sec_class = class_create(THIS_MODULE, "sec");
	 if (IS_ERR(sec_class))
                pr_err("Failed to create class(sec)!\n");

	 lcd->sec_lcdtype_dev = device_create(sec_class, NULL, 0, NULL, "sec_lcd");
	 if (IS_ERR(lcd->sec_lcdtype_dev))
	 	pr_err("Failed to create device(ts)!\n");

	 if (device_create_file(lcd->sec_lcdtype_dev, &dev_attr_lcdtype_file_cmd) < 0)
	 	pr_err("Failed to create device file(%s)!\n", dev_attr_lcdtype_file_cmd.attr.name);

#ifdef CONFIG_MACH_AEGIS
 	 if (device_create_file(lcd->sec_lcdtype_dev, &dev_attr_call_lcd) < 0)
	 	pr_err("Failed to create device file(%s)!\n", dev_attr_call_lcd.attr.name);
#endif
	
#ifdef CONFIG_FB_S3C_MDNIE
	init_mdnie_class();  //set mDNIe UI mode, Outdoormode
#endif

	spi_set_drvdata(spi, lcd);

//	tl2796_ldi_enable(lcd);
#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd->early_suspend.suspend = tl2796_early_suspend;
	lcd->early_suspend.resume = tl2796_late_resume;
	lcd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	register_early_suspend(&lcd->early_suspend);
#endif
	pr_info("tl2796_probe successfully proved\n");

	return 0;
err_setup_gammaset:
	lcd_device_unregister(lcd->lcd_dev);

err_setup_lcd:
	backlight_device_unregister(lcd->bl_dev);

err_setup:
	mutex_destroy(&lcd->lock);
	kfree(lcd);

err_alloc:
	return ret;
}

static int __devexit tl2796_remove(struct spi_device *spi)
{
	struct s5p_lcd *lcd = spi_get_drvdata(spi);

	unregister_early_suspend(&lcd->early_suspend);

	backlight_device_unregister(lcd->bl_dev);

	tl2796_ldi_disable(lcd);

	kfree(lcd);

	return 0;
}

static struct spi_driver tl2796_driver = {
	.driver = {
		.name	= "tl2796",
		.owner	= THIS_MODULE,
	},
	.probe		= tl2796_probe,
	.remove		= __devexit_p(tl2796_remove),
};

static int __init tl2796_init(void)
{
	return spi_register_driver(&tl2796_driver);
}
static void __exit tl2796_exit(void)
{
	spi_unregister_driver(&tl2796_driver);
}

module_init(tl2796_init);
module_exit(tl2796_exit);


MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("s6e63m0 LDI driver");
MODULE_LICENSE("GPL");
