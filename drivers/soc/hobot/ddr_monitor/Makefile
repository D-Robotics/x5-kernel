# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the horizon axi performance monitory device drivers.
#

obj-$(CONFIG_HOBOT_AXI_MONITOR) += axi_monitor_framework.o
axi_monitor_framework-objs	:=  horizon_axi_monitor.o \
								horizon_noc_qos.o
obj-$(CONFIG_HOBOT_AXI_MONITOR_DEBUGFS) += horizon_axi_monitor_debugfs.o
