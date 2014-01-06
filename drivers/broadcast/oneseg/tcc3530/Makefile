#
# Makefile for the ISDB-Tmm TCC3530 drivers.
#

# Each configuration option enables a list of files.
# LGE

CFLAGS_KERNEL+=-D_MODEL_F9J_

obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= broadcast_tcc3530_drv_if.o
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= broadcast_tcc353x.o
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= tcc353x_hal.o
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= tcc353x_irq.o
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= Tcc353xDriver/
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= broadcast_mmbi_chk_dev.o
obj-$(CONFIG_LGE_BROADCAST_ONESEG)		+= mbt_dataring.o
EXTRA_CFLAGS += -Idrivers/broadcast/oneseg
EXTRA_CFLAGS += -Idrivers/broadcast/oneseg/tcc3530/Tcc353xDriver/common
EXTRA_CFLAGS += -Idrivers/broadcast/oneseg/tcc3530/Tcc353xDriver/api/inc
EXTRA_CFLAGS += -Idrivers/broadcast/oneseg/tcc3530/Tcc353xDriver/PAL
EXTRA_CFLAGS += -Idrivers/broadcast/oneseg/tcc3530/Tcc353xDriver/sample/monitoring

