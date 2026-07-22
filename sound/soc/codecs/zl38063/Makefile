include $(srctree)/sound/soc/codecs/zl38063/config.mk

ccflags-y += -I$(srctree)/sound/soc/codecs/zl38063/include
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC)      += hbi.o
#ifeq ($(TARGET),TW)
#	obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += hbi_tw.o
#endif
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += hbi_ssl.o
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += hbi.o
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += ssl_port.o
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += hal_port.o
#obj-$(CONFIG_SND_SOC_ZL380XX_CODEC) += zl380xx_codec.o

snd-soc-zl380xx_codec-objs := hbi.o hbi_ssl.o hbi.o ssl_port.o hal_port.o zl380xx_codec.o hbi_k.o
ifeq ($(TARGET),TW)
	snd-soc-zl380xx_codec-objs += hbi_tw.o
endif
obj-$(CONFIG_SND_SOC_ZL38063) += snd-soc-zl380xx_codec.o
