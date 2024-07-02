#
# Makefile for the Hobot network device drivers.
#

obj-$(CONFIG_JPLUS_ETH) += hobot_eth.o
hobot_eth-objs += hobot_eth_jplus.o
hobot_eth-objs += hobot_eth_mmc.o
hobot_eth-objs += hobot_eth_jplus_tc.o
ifneq ($(CONFIG_JPLUS_ETH_FUSA),)
hobot_eth-objs += hobot_eth_stl.o
endif

ifeq ($(CONFIG_HOBOT_GCOV_BASE),y)
GCOV_PROFILE := y
endif
