obj-$(CONFIG_DVB_MESON)	+= meson_demux.o meson_fe.o

ccflags-y += -I$(srctree)/include/media \
             -I$(srctree)/drivers/media/pci/ttpci \
             -I$(srctree)/drivers/gpio -I$(srctree)/drivers/media \
             -I$(srctree)/include -DENABLE_DEMUX_DRIVER

meson_demux-objs += aml_dvb.o
meson_demux-objs += aml_dmx.o
meson_fe-objs    += aml_fe.o
