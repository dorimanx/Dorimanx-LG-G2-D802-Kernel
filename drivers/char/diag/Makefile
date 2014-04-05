obj-$(CONFIG_DIAG_CHAR) := diagchar.o
obj-$(CONFIG_DIAG_SDIO_PIPE) += diagfwd_sdio.o
obj-$(CONFIG_DIAGFWD_BRIDGE_CODE) += diagfwd_bridge.o
obj-$(CONFIG_DIAGFWD_BRIDGE_CODE) += diagfwd_hsic.o
obj-$(CONFIG_DIAGFWD_BRIDGE_CODE) += diagfwd_smux.o
#diagchar-objs := diagchar_core.o diagchar_hdlc.o diagfwd.o diagmem.o diagfwd_cntl.o diag_dci.o diag_masks.o diag_debugfs.o
# LGE Add: mts driver
diagchar-objs := diagchar_core.o diagchar_hdlc.o diagfwd.o diagmem.o diagfwd_cntl.o diag_dci.o diag_masks.o diag_debugfs.o diagchar_mts.o
obj-$(CONFIG_LGE_DM_DEV) += lg_dm_dev_tty.o
obj-$(CONFIG_LGE_DM_APP) += lg_dm_tty.o
