##########################################################
#
#	NFC Driver (FOR CXD2235BGG)
#
##########################################################


# Objects

SNFCOBJ := snfc_driver.o snfc_gpio.o snfc_i2c.o
SNFCOBJ += snfc_available_poll.o snfc_intu_poll.o snfc_rfs.o snfc_cen.o snfc_uart_collision_control.o
SNFCOBJ += snfc_hw_rev.o

# All selected in one module named snfc
snfc-objs := $(SNFCOBJ)

obj-$(CONFIG_LGE_NFC_SONY_KDDI) += snfc.o
