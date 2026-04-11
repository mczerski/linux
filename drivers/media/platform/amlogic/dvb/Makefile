obj-$(CONFIG_DVB_AMLOGIC) += amlogic-dvb.o amlogic-fe.o

amlogic-dvb-y := \
	amlogic-dvb-asyncfifo.o \
	amlogic-dvb-core.o      \
	amlogic-dvb-debugfs.o   \
	amlogic-dvb-demux.o     \
	amlogic-dvb-dsc.o       \
	amlogic-dvb-pts.o       \
	amlogic-dvb-regmap.o    \
	amlogic-dvb-stats.o     \
	amlogic-dvb-ts.o

amlogic-fe-y := amlogic-dvb-frontend.o

ccflags-y += -I$(src)
