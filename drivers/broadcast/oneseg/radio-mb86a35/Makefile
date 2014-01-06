##	radio-mb86a35-objs	:= radio-mb86a35.o
obj-y += radio-mb86a35s.o
obj-y += mbt_dataring.o
obj-y += isdbt_common.o
obj-y += broadcast_mmbi_chk_dev.o
##	CONFIG_RADIO_MB86A35=y
#
##	obj-$(CONFIG_RADIO_MB86A35) += radio-mb86a35.o
#
#ifeq ($(CONFIG_DEBUG_DRIVER),y)
EXTRA_CFLAGS += -DDEBUG
EXTRA_CFLAGS += -DKERNEL_2635
#endif
#
ifeq ($(ARCH),i386)
EXTRA_CFLAGS += -DDEBUG_I386
endif

#clean:
#	rm -rf *.o .radio-mb86a35s* radio-mb86a35s.ko radio-mb86a35s.mod.c modules.order Module.symvers .tmp_versions
