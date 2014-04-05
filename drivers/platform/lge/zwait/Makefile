obj-zwait-event-$(CONFIG_SWITCH) := zwait-event.o
zwait-objs := zwait-dev.o \
	      zwait-pwrkey.o \
	      zwait-power.o \
	      zwait-led.o \
	      zwait-rtc.o \
	      $(obj-zwait-event-y)

obj-$(CONFIG_ZERO_WAIT) += zwait.o
obj-$(CONFIG_ZERO_WAIT_DEBUGFS) += zwait-debugfs.o

subdir-ccflags-$(CONFIG_ZERO_WAIT) += -Idrivers/base/power \
				      -Idrivers/leds
subdir-ccflags-$(CONFIG_ZERO_WAIT_DEBUGFS) += -Ikernel/irq
