# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I${srctree}/include/

snd-soc-duplex-card-objs        := duplex-card.o
snd-soc-hobot-sound-machine-objs   := hobot-sound-machine.o
snd-soc-hobot-sound-duplex-host-objs   := hobot-sound-duplex-host.o

obj-$(CONFIG_SND_DUPLEX_CARD)           += snd-soc-duplex-card.o
obj-$(CONFIG_SND_HOBOT_SOUND_MACHINE)           += snd-soc-hobot-sound-machine.o
obj-$(CONFIG_SND_HOBOT_SOUND_DUPLEX_HOST)           += snd-soc-hobot-sound-duplex-host.o

