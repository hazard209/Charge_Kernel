#
# Makefile for the s3c framebuffer driver
#

ifeq ($(CONFIG_FB_S3C),y)
obj-y				+= s3cfb.o
obj-$(CONFIG_ARCH_S5PV210)	+= s3cfb_fimd6x.o

obj-$(CONFIG_FB_S3C_LTE480WV)	+= s3cfb_lte480wv.o
obj-$(CONFIG_FB_S3C_LVDS)       += s3cfb_lvds.o
obj-$(CONFIG_FB_S3C_HT101HD1)   += s3cfb_ht101hd1.o
ifeq ($(CONFIG_FB_S3C_CHIEF),y)
obj-$(CONFIG_FB_S3C_S6D16A0X)     += s3cfb_s6d16a0x.o
endif
obj-$(CONFIG_FB_S3C_TL2796)     += s3cfb_tl2796.o
obj-$(CONFIG_FB_S3C_S6D05A1X12) += s3cfb_s6d05a1x12.o
obj-$(CONFIG_FB_S3C_LD9040)     += ld9040.o
obj-$(CONFIG_FB_S3C_AMS701KA)	+= s3cfb_ams701ka.o
obj-$(CONFIG_FB_S3C_MDNIE)      += s3cfb_mdnie.o s3cfb_ielcd.o
obj-$(CONFIG_FB_S3C_CMC623)     += tune_cmc623.o
endif

ifeq ($(CONFIG_FB_S3C_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

